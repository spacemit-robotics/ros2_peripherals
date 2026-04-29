extern "C" {
#include <motor.h>
}

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <peripherals/msg/motor_command_array.hpp>
#include <peripherals/msg/motor_state_array.hpp>
#include <peripherals/srv/motor_get_param.hpp>
#include <peripherals/srv/motor_set_param.hpp>
#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/set_bool.hpp>

using std::chrono_literals::operator""s;

namespace {

/**
 * @brief Map MotorCommand.msg mode field to motor.h enum
 * motor_mode enum: 0=IDLE, 1=POS, 2=VEL, 3=TRQ, 4=HYBRID,
 *                  5=CSP, 6=CSV, 7=CST, 8=HM
 */
static uint32_t map_mode(uint8_t msg_mode) {
    switch (msg_mode) {
        case MOTOR_MODE_IDLE:
            return MOTOR_MODE_IDLE;
        case MOTOR_MODE_POS:
            return MOTOR_MODE_POS;
        case MOTOR_MODE_VEL:
            return MOTOR_MODE_VEL;
        case MOTOR_MODE_TRQ:
            return MOTOR_MODE_TRQ;
        case MOTOR_MODE_HYBRID:
            return MOTOR_MODE_HYBRID;
        case MOTOR_MODE_CSP:
            return MOTOR_MODE_CSP;
        case MOTOR_MODE_CSV:
            return MOTOR_MODE_CSV;
        case MOTOR_MODE_CST:
            return MOTOR_MODE_CST;
        case MOTOR_MODE_HM:
            return MOTOR_MODE_HM;
        default:
            return MOTOR_MODE_IDLE;
    }
}

static motor_cmd to_motor_cmd(const peripherals::msg::MotorCommand& cmd_msg) {
    motor_cmd cmd{};
    cmd.mode = map_mode(cmd_msg.mode);
    cmd.pos_des = static_cast<float>(cmd_msg.pos_des);
    cmd.vel_des = static_cast<float>(cmd_msg.vel_des);
    cmd.trq_des = static_cast<float>(cmd_msg.trq_des);
    cmd.kp = static_cast<float>(cmd_msg.kp);
    cmd.kd = static_cast<float>(cmd_msg.kd);
    return cmd;
}

static std::vector<double> normalize_vec_param(
    const std::vector<double>& v,
    size_t target_size,
    const std::string& name) {
    if (v.size() == target_size) {
        return v;
    }
    if (v.size() == 1 && target_size > 1) {
        return std::vector<double>(target_size, v[0]);
    }
    if (v.empty() && target_size > 0) {
        return std::vector<double>(target_size, 0.0);
    }
    throw std::runtime_error(
        "Parameter '" + name + "' size mismatch: got " + std::to_string(v.size()) +
        ", expected " + std::to_string(target_size) + " (or 1)");
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

}  // namespace

class MotorNode final : public rclcpp::Node {
public:
    MotorNode() : rclcpp::Node("motor_node"), enabled_(false) {
        // 启动节点时自动加载 yaml 文件中的参数, declare_parameter 从参数服务器读取这些值
        motor_type_ = declare_parameter<std::string>("motor_type", "can");  // can, uart, ecat
        driver_name_ = declare_parameter<std::string>(
            "driver_name", "drv_can_dm");  // 注册驱动时的名字
        motor_iface_ = declare_parameter<std::string>("motor_iface", "can0");
        motor_ids_ = declare_parameter<std::vector<int64_t>>("motor_ids", {1});
        baud_ = declare_parameter<int>("baud", 1000000);
        slave_indices_ = declare_parameter<std::vector<int64_t>>("slave_indices", std::vector<int64_t>{});

        // EtherCAT specific settings
        ecat_cycle_ms_ = declare_parameter<int>("ecat_cycle_ms", 2);
        wait_for_ready_ = declare_parameter<bool>("wait_for_ready", true);

        // Control parameters (dynamic)
        // For "can" type (MIT mode), these are Kp, Ki (if used), Kd
        // For "ecat" type, these are generally ignored in favor of profile parameters
        pid_kp_ = declare_parameter<std::vector<double>>("pid_kp", {1.0});
        pid_ki_ = declare_parameter<std::vector<double>>("pid_ki", {0.1});
        pid_kd_ = declare_parameter<std::vector<double>>("pid_kd", {0.01});
        max_velocity_ = declare_parameter<double>("max_velocity", 10.0);
        max_torque_ = declare_parameter<double>("max_torque", 5.0);

        // Profile parameters (for ECAT PP mode defaults)
        profile_vel_ = declare_parameter<std::vector<double>>("profile_vel", {100000.0});
        profile_acc_ = declare_parameter<std::vector<double>>("profile_acc", {100000.0});
        profile_dec_ = declare_parameter<std::vector<double>>("profile_dec", {100000.0});

        // Other settings
        state_publish_hz_ = declare_parameter<double>("state_publish_hz", 100.0);
        cmd_timeout_ms_ = declare_parameter<int>("cmd_timeout_ms", 200);

        if (motor_ids_.empty()) {
            throw std::runtime_error("motor_ids is empty");
        }

        devs_.reserve(motor_ids_.size());
        id_to_index_.reserve(motor_ids_.size());

        // Normalize arrays
        pid_kp_ = normalize_vec_param(pid_kp_, motor_ids_.size(), "pid_kp");
        pid_ki_ = normalize_vec_param(pid_ki_, motor_ids_.size(), "pid_ki");
        pid_kd_ = normalize_vec_param(pid_kd_, motor_ids_.size(), "pid_kd");
        profile_vel_ = normalize_vec_param(profile_vel_, motor_ids_.size(), "profile_vel");
        profile_acc_ = normalize_vec_param(profile_acc_, motor_ids_.size(), "profile_acc");
        profile_dec_ = normalize_vec_param(profile_dec_, motor_ids_.size(), "profile_dec");

        // Initialize cycle time storage for ECAT
        uint32_t common_cycle_ms = static_cast<uint32_t>(ecat_cycle_ms_);

        for (size_t i = 0; i < motor_ids_.size(); ++i) {
            const auto id = static_cast<uint32_t>(motor_ids_[i]);
            motor_dev* dev = nullptr;

            if (motor_type_ == "can") {
                dev = motor_alloc_can(driver_name_.c_str(), motor_iface_.c_str(), id, nullptr);
            } else if (motor_type_ == "uart") {
                dev = motor_alloc_uart(driver_name_.c_str(),
                                        motor_iface_.c_str(),
                                        static_cast<uint32_t>(baud_),
                                        static_cast<uint8_t>(id),
                                        nullptr);
            } else if (motor_type_ == "ecat") {
                // slave_idx refers to EtherCAT bus position
                uint16_t slave_idx =
                    (i < slave_indices_.size()) ? static_cast<uint16_t>(slave_indices_[i]) :
                                                static_cast<uint16_t>(i);

                // For JMC EtherCAT, the third argument is simply the cycle_ms (as a uint32_t pointer)
                dev = motor_alloc_ecat(driver_name_.c_str(), slave_idx, &common_cycle_ms);
            } else {
                throw std::runtime_error("Unsupported motor_type: " + motor_type_);
            }

            if (!dev) {
                throw std::runtime_error("motor_alloc failed for " + motor_type_ + " driver=" + driver_name_ +
                                        " id=" + std::to_string(id));
            }
            devs_.push_back(dev);
            id_to_index_[id] = i;
        }

        if (motor_init(devs_.data(), static_cast<uint32_t>(devs_.size())) != 0) {
            throw std::runtime_error("motor_init failed");
        }

        last_cmds_.resize(devs_.size());
        set_all_idle();
        last_cmd_time_ = now();

        // If EtherCAT, wait for ready and anchor logic zero
        if (motor_type_ == "ecat" && wait_for_ready_) {
            wait_for_ecat_ready();
        }

        // Register parameter callback
        // 动态参数回调，运行时修改参数
        // -can 类型：pid_kp, pid_ki, pid_kd
        // -ecat 类型：profile_vel, profile_acc, profile_dec
        param_cb_handle_ =
            add_on_set_parameters_callback(std::bind(&MotorNode::on_set_parameters, this, std::placeholders::_1));

        // 创建指令订阅者，每当有新的指令发来时，触发回调 on_cmd()，节点会解析这些指令并下发给底层电机驱动
        cmd_sub_ = create_subscription<peripherals::msg::MotorCommandArray>(
            "/cmd/motor", rclcpp::QoS(10).reliable(), std::bind(&MotorNode::on_cmd, this, std::placeholders::_1));

        // 创建状态发布者，将从底层硬件读取到的电机实时数据打包并发布到 ROS 网络中
        state_pub_ = create_publisher<peripherals::msg::MotorStateArray>("/motor/state", rclcpp::QoS(10).reliable());

        // 创建使能服务，用于使能或禁用电机
        enable_srv_ = create_service<std_srvs::srv::SetBool>(
            "/motor/enable", std::bind(&MotorNode::on_enable, this, std::placeholders::_1, std::placeholders::_2));

        // 创建参数写入服务：/motor/set_param
        set_param_srv_ = create_service<peripherals::srv::MotorSetParam>(
            "/motor/set_param",
            std::bind(&MotorNode::on_set_motor_param, this, std::placeholders::_1, std::placeholders::_2));

        // 创建参数读取服务：/motor/get_param
        get_param_srv_ = create_service<peripherals::srv::MotorGetParam>(
            "/motor/get_param",
            std::bind(&MotorNode::on_get_motor_param, this, std::placeholders::_1, std::placeholders::_2));

        // 创建状态定时器，按照 state_publish_hz_ 的频率发布电机状态
        const auto period = std::chrono::duration<double>(1.0 / std::max(1.0, state_publish_hz_));
        state_timer_ = create_wall_timer(std::chrono::duration_cast<std::chrono::nanoseconds>(period),
                                        std::bind(&MotorNode::publish_state, this));

        RCLCPP_INFO(get_logger(), "motor_node ready: type=%s driver=%s iface=%s motors=%zu", motor_type_.c_str(),
                    driver_name_.c_str(), motor_iface_.c_str(), motor_ids_.size());
    }

    ~MotorNode() override {
        if (!devs_.empty()) {
            motor_free(devs_.data(), static_cast<uint32_t>(devs_.size()));
            devs_.clear();
        }
    }

private:
    void wait_for_ecat_ready() {
        RCLCPP_INFO(get_logger(), "Waiting for EtherCAT motors to enable and anchor...");

        auto start_time = now();
        const auto timeout = 15.0s;
        int stable_counts = 0;
        const int required_stable = 100 / std::max(1, ecat_cycle_ms_);

        std::vector<motor_state> states(devs_.size());
        std::vector<motor_cmd> init_cmds(devs_.size());
        for (size_t i = 0; i < init_cmds.size(); ++i) {
            init_cmds[i].mode = MOTOR_MODE_POS;  // Default mode for anchoring
            init_cmds[i].pos_des = 0.0f;
        }

        rclcpp::WallRate rate(1000.0 / std::max(1, ecat_cycle_ms_));

        while (rclcpp::ok()) {
            if (now() - start_time > timeout) {
                RCLCPP_WARN(get_logger(),
                            "Timeout waiting for EtherCAT motors. Some "
                            "motors might not be ready.");
                break;
            }

            if (motor_get_states(devs_.data(), states.data(), static_cast<uint32_t>(devs_.size())) != 0) {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Failed to get motor states during ECAT init");
            }

            bool all_enabled = true;
            for (const auto& s : states) {
                // SW Bit 0-6 check for Operational/Running state (0x27)
                if ((static_cast<uint16_t>(s.err) & 0x006F) != 0x0027) {
                    all_enabled = false;
                    break;
                }
            }

            // Proactively send pos_des=0 in POS mode to trigger driver's anchoring
            // logic
            motor_set_cmds(devs_.data(), init_cmds.data(), static_cast<uint32_t>(devs_.size()));

            if (all_enabled) {
                if (++stable_counts >= required_stable) {
                    RCLCPP_INFO(get_logger(), "All EtherCAT motors ready and anchored at physics-zero.");
                    break;
                }
            } else {
                stable_counts = 0;
                if (static_cast<int>((now() - start_time).seconds()) % 2 == 0) {
                    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "Still waiting for enable... (M0 SW=0x%04X)",
                                        static_cast<uint16_t>(states[0].err));
                }
            }

            rate.sleep();
        }
    }

    bool is_mode_supported(uint32_t mode) {
        if (mode == MOTOR_MODE_IDLE) {
            return true;
        }
        if (motor_type_ == "can") {
            // 达妙 CAN 支持全部 4 种模式（底层 damiao_pack.cpp 均有实现）：
            // HYBRID → MIT 阻抗控制
            // POS    → POS_VEL 位置+速度控制
            // VEL    → VEL_MODE 纯速度控制
            // TRQ    → POS_FORCE_MODE 力位混控
            return mode == MOTOR_MODE_HYBRID || mode == MOTOR_MODE_POS || mode == MOTOR_MODE_VEL ||
                    mode == MOTOR_MODE_TRQ;
        } else if (motor_type_ == "uart") {
            // uart 支持 速度和位置两种模式
            return mode == MOTOR_MODE_POS || mode == MOTOR_MODE_VEL;
        } else if (motor_type_ == "ecat") {
            // Ethercat 支持 CSP, CSV, CST, HM, POS(PP), VEL(PV), TRQ(PT)
            return mode == MOTOR_MODE_POS || mode == MOTOR_MODE_VEL || mode == MOTOR_MODE_TRQ ||
                    mode == MOTOR_MODE_CSP || mode == MOTOR_MODE_CSV || mode == MOTOR_MODE_CST || mode == MOTOR_MODE_HM;
        }
        return false;
    }

    // 动态参数回调函数，外部通过 ros2 param set 修改参数时触发
    rcl_interfaces::msg::SetParametersResult on_set_parameters(const std::vector<rclcpp::Parameter>& parameters) {
        auto result = rcl_interfaces::msg::SetParametersResult();
        result.successful = true;

        for (const auto& parameter : parameters) {
            const auto& name = parameter.get_name();
            try {
                if (name == "pid_kp") {
                    pid_kp_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), "pid_kp");
                } else if (name == "pid_ki") {
                    pid_ki_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), "pid_ki");
                } else if (name == "pid_kd") {
                    pid_kd_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), "pid_kd");
                } else if (name == "max_velocity") {
                    max_velocity_ = parameter.as_double();
                } else if (name == "max_torque") {
                    max_torque_ = parameter.as_double();
                } else if (name == "cmd_timeout_ms") {
                    cmd_timeout_ms_ = static_cast<int>(parameter.as_int());
                } else if (name == "profile_vel" || name == "profile_acc" || name == "profile_dec") {
                    // Allow dynamic update of initial defaults (will be applied on next
                    // set_cmd or could be pushed via set_paras)
                    if (name == "profile_vel") {
                        profile_vel_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), name);
                    }
                    if (name == "profile_acc") {
                        profile_acc_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), name);
                    }
                    if (name == "profile_dec") {
                        profile_dec_ = normalize_vec_param(parameter.as_double_array(), motor_ids_.size(), name);
                    }
                } else if (name == "motor_type" || name == "driver_name" || name == "motor_iface" ||
                            name == "motor_ids" || name == "baud" || name == "slave_indices" ||
                            name == "ecat_cycle_ms" || name == "wait_for_ready") {
                    result.successful = false;
                    result.reason = "Parameter '" + name + "' cannot be modified at runtime";
                }
            } catch (const std::exception& e) {
                result.successful = false;
                result.reason = e.what();
            }
        }
        return result;
    }

    void set_all_idle() {
        for (auto& c : last_cmds_) {
            c.mode = MOTOR_MODE_IDLE;
            c.pos_des = 0.0f;
            c.vel_des = 0.0f;
            c.trq_des = 0.0f;
            c.kp = 0.0f;
            c.kd = 0.0f;
        }
    }

    // 运行时停车并发送指令
    void send_all_idle() {
        set_all_idle();
        (void)motor_set_cmds(devs_.data(), last_cmds_.data(), static_cast<uint32_t>(devs_.size()));
    }

    // 超时安全停车：保持当前模式，仅将运动目标归零，避免 EtherCAT 模式震荡
    void safe_stop() {
        for (auto& c : last_cmds_) {
            c.vel_des = 0.0f;
            c.trq_des = 0.0f;
        }
        (void)motor_set_cmds(devs_.data(), last_cmds_.data(), static_cast<uint32_t>(devs_.size()));
    }

    void on_enable(const std::shared_ptr<std_srvs::srv::SetBool::Request> req,
                    std::shared_ptr<std_srvs::srv::SetBool::Response> resp) {
        const bool was_enabled = enabled_;
        enabled_ = req->data;
        if (!enabled_) {
            // 禁用：将所有指令归零（不施加控制力）
            send_all_idle();
        } else if (!was_enabled) {
            // 从 disabled -> enabled：重置超时计时以防立刻触发 safe_stop
            last_cmd_time_ = now();
        }
        RCLCPP_INFO(get_logger(), "Motor enable: %s -> %s", was_enabled ? "true" : "false",
                    enabled_ ? "true" : "false");
        resp->success = true;
        resp->message = enabled_ ? "enabled" : "disabled";
    }

    // 写电机寄存器参数：/motor/set_param
    void on_set_motor_param(const std::shared_ptr<peripherals::srv::MotorSetParam::Request> req,
                            std::shared_ptr<peripherals::srv::MotorSetParam::Response> resp) {
        auto it = id_to_index_.find(req->motor_id);
        if (it == id_to_index_.end()) {
            resp->success = false;
            resp->message = "Unknown motor_id: " + std::to_string(req->motor_id);
            RCLCPP_WARN(get_logger(), "set_param: %s", resp->message.c_str());
            return;
        }
        const auto idx = it->second;
        float value = req->value;
        // address 编码为寄存器号，通过 uintptr_t 传递给驱动
        const void* addr = reinterpret_cast<const void*>(static_cast<uintptr_t>(req->reg_address));
        int ret = motor_set_paras(devs_[idx], addr, &value, sizeof(float));
        if (ret == 0) {
            resp->success = true;
            resp->message = "OK";
            RCLCPP_INFO(get_logger(), "set_param motor_id=%u reg=0x%X value=%.4f -> OK", req->motor_id,
                        req->reg_address, static_cast<double>(value));
        } else {
            resp->success = false;
            resp->message = "motor_set_paras failed (ret=" + std::to_string(ret) + ")";
            RCLCPP_WARN(get_logger(), "set_param motor_id=%u reg=0x%X: %s", req->motor_id, req->reg_address,
                        resp->message.c_str());
        }
    }

    // 读电机寄存器参数：/motor/get_param
    void on_get_motor_param(const std::shared_ptr<peripherals::srv::MotorGetParam::Request> req,
                            std::shared_ptr<peripherals::srv::MotorGetParam::Response> resp) {
        auto it = id_to_index_.find(req->motor_id);
        if (it == id_to_index_.end()) {
            resp->success = false;
            resp->value = 0.0f;
            resp->message = "Unknown motor_id: " + std::to_string(req->motor_id);
            RCLCPP_WARN(get_logger(), "get_param: %s", resp->message.c_str());
            return;
        }
        const auto idx = it->second;
        float value = 0.0f;
        const void* addr = reinterpret_cast<const void*>(static_cast<uintptr_t>(req->reg_address));
        int ret = motor_get_paras(devs_[idx], addr, &value, sizeof(float));
        if (ret == 0) {
            resp->success = true;
            resp->value = value;
            resp->message = "OK";
            RCLCPP_INFO(get_logger(), "get_param motor_id=%u reg=0x%X -> %.4f", req->motor_id, req->reg_address,
                        static_cast<double>(value));
        } else {
            resp->success = false;
            resp->value = 0.0f;
            resp->message = "motor_get_paras failed (ret=" + std::to_string(ret) + ")";
            RCLCPP_WARN(get_logger(), "get_param motor_id=%u reg=0x%X: %s", req->motor_id, req->reg_address,
                        resp->message.c_str());
        }
    }

    void on_cmd(const peripherals::msg::MotorCommandArray& msg) {
        if (!enabled_) {
            return;
        }

        for (const auto& cmd_msg : msg.commands) {
            const uint32_t id = cmd_msg.id;
            auto it = id_to_index_.find(id);
            if (it == id_to_index_.end()) {
                continue;
            }
            const auto idx = it->second;
            auto cmd = to_motor_cmd(cmd_msg);

            // Validate mode support
            if (!is_mode_supported(cmd.mode)) {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                                    "Motor %u: mode %u not supported by %s motor "
                                    "type. Skipping control for this motor.",
                                    id, cmd.mode, motor_type_.c_str());
                continue;
            }

            // Apply limits from params
            // 速度、力矩限制，防止误发过大指令损坏设备
            cmd.vel_des = clampf(cmd.vel_des, static_cast<float>(-max_velocity_), static_cast<float>(max_velocity_));
            cmd.trq_des = clampf(cmd.trq_des, static_cast<float>(-max_torque_), static_cast<float>(max_torque_));

            // ethercat 电机参数映射 kp -- > profile_acc, kd --> profile_dec
            if (cmd.kp == 0.0f) {
                cmd.kp = static_cast<float>(motor_type_ == "ecat" ? profile_acc_[idx] : pid_kp_[idx]);
            }
            if (cmd.kd == 0.0f) {
                cmd.kd = static_cast<float>(motor_type_ == "ecat" ? profile_dec_[idx] : pid_kd_[idx]);
            }

            last_cmds_[idx] = cmd;
        }

        if (motor_set_cmds(devs_.data(), last_cmds_.data(), static_cast<uint32_t>(devs_.size())) != 0) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "motor_set_cmds failed");
        }
        last_cmd_time_ = now();
    }

    void publish_state() {
        // Timeout -> safe stop (preserve mode, zero motion targets)
        if (enabled_ && cmd_timeout_ms_ > 0) {
            const auto dt = (now() - last_cmd_time_).nanoseconds() / 1000000LL;
            if (dt > cmd_timeout_ms_) {
                safe_stop();
                last_cmd_time_ = now();
            }
        }

        std::vector<motor_state> states(devs_.size());
        if (motor_get_states(devs_.data(), states.data(), static_cast<uint32_t>(devs_.size())) != 0) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "motor_get_states failed");
        }

        peripherals::msg::MotorStateArray out;
        out.header.stamp = now();
        out.header.frame_id = "base_link";
        out.states.reserve(states.size());

        for (size_t i = 0; i < states.size(); ++i) {
            peripherals::msg::MotorState s;
            s.id = static_cast<uint32_t>(motor_ids_[i]);
            s.pos = states[i].pos;
            s.vel = states[i].vel;
            s.trq = states[i].trq;
            s.temp = states[i].temp;
            s.error_flags = states[i].err;
            out.states.push_back(s);
        }

        state_pub_->publish(out);
    }

    // Params (Hardware - Static)
    std::string motor_type_;
    std::string driver_name_;
    std::string motor_iface_;
    std::vector<int64_t> motor_ids_;
    int baud_{1000000};
    std::vector<int64_t> slave_indices_;
    int ecat_cycle_ms_{2};
    bool wait_for_ready_{true};

    // Params (Control - Dynamic)
    std::vector<double> pid_kp_;
    std::vector<double> pid_ki_;
    std::vector<double> pid_kd_;
    double max_velocity_{10.0};
    double max_torque_{5.0};
    std::vector<double> profile_vel_;
    std::vector<double> profile_acc_;
    std::vector<double> profile_dec_;

    // Params (Others)
    double state_publish_hz_{100.0};
    int cmd_timeout_ms_{200};

    // Motor devices
    std::vector<motor_dev*> devs_;
    std::unordered_map<uint32_t, size_t> id_to_index_;
    std::vector<motor_cmd> last_cmds_;
    rclcpp::Time last_cmd_time_{0, 0, RCL_ROS_TIME};
    bool enabled_;

    // ROS
    rclcpp::Subscription<peripherals::msg::MotorCommandArray>::SharedPtr cmd_sub_;
    rclcpp::Publisher<peripherals::msg::MotorStateArray>::SharedPtr state_pub_;
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr enable_srv_;
    rclcpp::Service<peripherals::srv::MotorSetParam>::SharedPtr set_param_srv_;
    rclcpp::Service<peripherals::srv::MotorGetParam>::SharedPtr get_param_srv_;
    rclcpp::TimerBase::SharedPtr state_timer_;
    OnSetParametersCallbackHandle::SharedPtr param_cb_handle_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<MotorNode>());
    } catch (const std::exception& e) { fprintf(stderr, "motor_node exception: %s\n", e.what()); }
    rclcpp::shutdown();
    return 0;
}
