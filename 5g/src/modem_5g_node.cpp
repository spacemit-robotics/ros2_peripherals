extern "C" {
#include <5g.h>
}

#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <peripherals_5g_node/msg/modem5g_ip_info.hpp>
#include <peripherals_5g_node/msg/modem5g_pdp_context.hpp>
#include <peripherals_5g_node/msg/modem5g_status.hpp>
#include <peripherals_5g_node/srv/modem5g_data_call.hpp>
#include <peripherals_5g_node/srv/modem5g_get_pdp_context.hpp>
#include <peripherals_5g_node/srv/modem5g_send_at.hpp>
#include <peripherals_5g_node/srv/modem5g_set_flight_mode.hpp>
#include <peripherals_5g_node/srv/modem5g_set_pdp_context.hpp>
#include <peripherals_5g_node/srv/modem5g_set_prefer_rat.hpp>
#include <peripherals_5g_node/srv/modem5g_trigger.hpp>

namespace
{
    constexpr uint8_t kMinCid = 1;
    constexpr uint8_t kMaxCid = 20;
    constexpr int kDefaultStatusPeriodMs = 2000;
    constexpr uint32_t kDefaultAtTimeoutMs = 2000;
    constexpr int kDefaultAtResponseMaxBytes = 1024;

    bool fits_fixed_field(const std::string & value, size_t max_len)
    {
        return value.size() <= max_len;
    }

    std::string describe_status(enum modem_5g_status status)
    {
        switch (status) {
            case MODEM_5G_STATUS_SUCCESS:
                return "success";
            case MODEM_5G_STATUS_FAIL:
                return "fail";
            case MODEM_5G_STATUS_NOT_READY:
                return "not ready";
            case MODEM_5G_STATUS_NOMEM:
                return "no memory";
            case MODEM_5G_STATUS_BUSY:
                return "busy";
            case MODEM_5G_STATUS_UNSUPPORTED:
                return "unsupported";
            case MODEM_5G_STATUS_INVALID:
                return "invalid";
            case MODEM_5G_STATUS_TIMEOUT:
                return "timeout";
            default:
                return "unknown(" + std::to_string(static_cast<int>(status)) + ")";
        }
    }

    template<size_t N>
    void copy_string_to_c_array(const std::string & src, char (&dst)[N])
    {
        std::snprintf(dst, N, "%s", src.c_str());
    }

    uint8_t to_ros_power_state(enum modem_5g_power_state state)
    {
        return static_cast<uint8_t>(state);
    }

    uint8_t to_ros_sim_state(enum modem_5g_sim_state state)
    {
        return static_cast<uint8_t>(state);
    }

    uint8_t to_ros_reg_state(enum modem_5g_reg_state state)
    {
        return static_cast<uint8_t>(state);
    }

    uint8_t to_ros_rat(enum modem_5g_rat rat)
    {
        return static_cast<uint8_t>(rat);
    }

    uint8_t to_ros_pdp_type(enum modem_5g_pdp_type type)
    {
        return static_cast<uint8_t>(type);
    }

    uint8_t to_ros_data_state(enum modem_5g_data_state state)
    {
        return static_cast<uint8_t>(state);
    }

    enum modem_5g_rat rat_from_request(uint8_t value)
    {
        switch (value) {
            case MODEM_5G_RAT_NR5G_SA:
                return MODEM_5G_RAT_NR5G_SA;
            case MODEM_5G_RAT_NR5G_NSA:
                return MODEM_5G_RAT_NR5G_NSA;
            case MODEM_5G_RAT_LTE:
                return MODEM_5G_RAT_LTE;
            case MODEM_5G_RAT_WCDMA:
                return MODEM_5G_RAT_WCDMA;
            case MODEM_5G_RAT_GSM:
                return MODEM_5G_RAT_GSM;
            default:
                return MODEM_5G_RAT_UNKNOWN;
        }
    }

    enum modem_5g_pdp_type pdp_type_from_request(uint8_t value)
    {
        switch (value) {
            case MODEM_5G_PDP_IPV4:
                return MODEM_5G_PDP_IPV4;
            case MODEM_5G_PDP_IPV6:
                return MODEM_5G_PDP_IPV6;
            case MODEM_5G_PDP_IPV4V6:
                return MODEM_5G_PDP_IPV4V6;
            default:
                return MODEM_5G_PDP_UNKNOWN;
        }
    }

    peripherals_5g_node::msg::Modem5gPdpContext to_ros_pdp_context(const struct modem_5g_pdp_context & context)
    {
        peripherals_5g_node::msg::Modem5gPdpContext msg;
        msg.cid = context.cid;
        msg.pdp_type = to_ros_pdp_type(context.pdp_type);
        msg.apn = context.apn;
        msg.username = context.username;
        msg.has_password = context.password[0] != '\0';
        return msg;
    }

    peripherals_5g_node::msg::Modem5gIpInfo to_ros_ip_info(const struct modem_5g_ip_info & info)
    {
        peripherals_5g_node::msg::Modem5gIpInfo msg;
        msg.ip = info.ip;
        msg.gateway = info.gateway;
        msg.dns1 = info.dns1;
        msg.dns2 = info.dns2;
        return msg;
    }

    template<typename ResponseT>
    void set_service_result(
        ResponseT * response, enum modem_5g_status rc, const std::string & action)
    {
        response->success = (rc == MODEM_5G_STATUS_SUCCESS);
        response->status_code = static_cast<int32_t>(rc);
        response->message = action + ": " + describe_status(rc);
    }
}  // namespace

class Modem5gNode final : public rclcpp::Node
{
    public:
        Modem5gNode()
            : rclcpp::Node("modem_5g_node")
        {
            try {
                name_ = declare_parameter<std::string>("name", "MR880A:mr880a0");
                uart_device_ = declare_parameter<std::string>("uart_device", "auto");
                baud_ = declare_parameter<int>("baud", 9600);
                frame_id_ = declare_parameter<std::string>("frame_id", "modem_5g");

                status_topic_ = declare_parameter<std::string>("status_topic", "modem_5g/status");
                power_on_service_name_ = declare_parameter<std::string>("power_on_service", "modem_5g/power_on");
                power_off_service_name_ = declare_parameter<std::string>("power_off_service", "modem_5g/power_off");
                reset_service_name_ = declare_parameter<std::string>("reset_service", "modem_5g/reset");
                flight_mode_service_name_ = declare_parameter<std::string>(
                        "flight_mode_service", "modem_5g/set_flight_mode");
                prefer_rat_service_name_ = declare_parameter<std::string>(
                        "prefer_rat_service", "modem_5g/set_prefer_rat");
                set_pdp_service_name_ = declare_parameter<std::string>(
                        "set_pdp_service", "modem_5g/set_pdp_context");
                get_pdp_service_name_ = declare_parameter<std::string>(
                        "get_pdp_service", "modem_5g/get_pdp_context");
                data_call_service_name_ = declare_parameter<std::string>(
                        "data_call_service", "modem_5g/data_call");
                send_at_service_name_ = declare_parameter<std::string>(
                        "send_at_service", "modem_5g/send_at");

                publish_on_startup_ = declare_parameter<bool>("publish_on_startup", true);
                status_period_ms_ = declare_parameter<int>("status_period_ms", kDefaultStatusPeriodMs);

                default_cid_ = declare_parameter<int>("default_cid", 1);
                default_pdp_type_ = declare_parameter<int>(
                        "default_pdp_type", static_cast<int>(MODEM_5G_PDP_IPV4V6));
                default_apn_ = declare_parameter<std::string>("default_apn", "");
                default_username_ = declare_parameter<std::string>("default_username", "");
                default_password_ = declare_parameter<std::string>("default_password", "");

                power_on_on_startup_ = declare_parameter<bool>("power_on_on_startup", false);
                apply_default_pdp_on_startup_ = declare_parameter<bool>(
                        "apply_default_pdp_on_startup", false);
                start_data_on_startup_ = declare_parameter<bool>("start_data_on_startup", false);

                at_timeout_ms_ = declare_parameter<int>("at_timeout_ms", static_cast<int>(kDefaultAtTimeoutMs));
                at_response_max_bytes_ = declare_parameter<int>(
                        "at_response_max_bytes", kDefaultAtResponseMaxBytes);

                validate_config();
                active_cid_ = static_cast<uint8_t>(default_cid_);

                dev_ = modem_5g_alloc_uart(name_.c_str(), uart_device_.c_str(), static_cast<uint32_t>(baud_));
                if (dev_ == nullptr) {
                    throw std::runtime_error("modem_5g_alloc_uart returned null");
                }

                enum modem_5g_status rc = modem_5g_init(dev_);
                if (rc != MODEM_5G_STATUS_SUCCESS) {
                    throw std::runtime_error("modem_5g_init failed: " + describe_status(rc));
                }

                {
                    std::lock_guard<std::mutex> lock(api_mutex_);
                    (void)refresh_basic_info_locked();

                    if (power_on_on_startup_) {
                        rc = modem_5g_power_on(dev_);
                        if (rc != MODEM_5G_STATUS_SUCCESS) {
                            throw std::runtime_error("modem_5g_power_on failed: " + describe_status(rc));
                        }
                        flight_mode_enabled_ = false;
                    }

                    if (apply_default_pdp_on_startup_) {
                        rc = apply_default_pdp_context_locked();
                        if (rc != MODEM_5G_STATUS_SUCCESS) {
                            throw std::runtime_error("apply default PDP context failed: " + describe_status(rc));
                        }
                    }

                    if (start_data_on_startup_) {
                        rc = modem_5g_data_start(dev_, static_cast<uint8_t>(default_cid_));
                        if (rc != MODEM_5G_STATUS_SUCCESS) {
                            throw std::runtime_error("modem_5g_data_start failed: " + describe_status(rc));
                        }
                        active_cid_ = static_cast<uint8_t>(default_cid_);
                    }
                }

                status_pub_ = create_publisher<peripherals_5g_node::msg::Modem5gStatus>(
                        status_topic_, rclcpp::QoS(1).reliable().transient_local());

                power_on_srv_ = create_service<peripherals_5g_node::srv::Modem5gTrigger>(
                        power_on_service_name_,
                        std::bind(&Modem5gNode::on_power_on, this, std::placeholders::_1, std::placeholders::_2));
                power_off_srv_ = create_service<peripherals_5g_node::srv::Modem5gTrigger>(
                        power_off_service_name_,
                        std::bind(&Modem5gNode::on_power_off, this, std::placeholders::_1, std::placeholders::_2));
                reset_srv_ = create_service<peripherals_5g_node::srv::Modem5gTrigger>(
                        reset_service_name_,
                        std::bind(&Modem5gNode::on_reset, this, std::placeholders::_1, std::placeholders::_2));
                flight_mode_srv_ = create_service<peripherals_5g_node::srv::Modem5gSetFlightMode>(
                        flight_mode_service_name_,
                        std::bind(
                            &Modem5gNode::on_set_flight_mode, this, std::placeholders::_1,
                            std::placeholders::_2));
                prefer_rat_srv_ = create_service<peripherals_5g_node::srv::Modem5gSetPreferRat>(
                        prefer_rat_service_name_,
                        std::bind(&Modem5gNode::on_set_prefer_rat, this, std::placeholders::_1, std::placeholders::_2));
                set_pdp_srv_ = create_service<peripherals_5g_node::srv::Modem5gSetPdpContext>(
                        set_pdp_service_name_,
                        std::bind(
                            &Modem5gNode::on_set_pdp_context, this, std::placeholders::_1,
                            std::placeholders::_2));
                get_pdp_srv_ = create_service<peripherals_5g_node::srv::Modem5gGetPdpContext>(
                        get_pdp_service_name_,
                        std::bind(
                            &Modem5gNode::on_get_pdp_context, this, std::placeholders::_1,
                            std::placeholders::_2));
                data_call_srv_ = create_service<peripherals_5g_node::srv::Modem5gDataCall>(
                        data_call_service_name_,
                        std::bind(&Modem5gNode::on_data_call, this, std::placeholders::_1, std::placeholders::_2));
                send_at_srv_ = create_service<peripherals_5g_node::srv::Modem5gSendAt>(
                        send_at_service_name_,
                        std::bind(&Modem5gNode::on_send_at, this, std::placeholders::_1, std::placeholders::_2));

                if (publish_on_startup_) {
                    publish_status_snapshot();
                }

                status_timer_ = create_wall_timer(
                        std::chrono::milliseconds(status_period_ms_),
                        std::bind(&Modem5gNode::publish_status_snapshot, this));

                RCLCPP_INFO(
                        get_logger(),
                        "modem_5g_node ready: name=%s uart_device=%s baud=%d status_topic=%s",
                        name_.c_str(), uart_device_.c_str(), baud_, status_topic_.c_str());
            } catch (...) {
                cleanup_resources();
                throw;
            }
        }

        ~Modem5gNode() override
        {
            cleanup_resources();
        }

    private:
        void validate_config() const
        {
            if (name_.empty()) {
                throw std::invalid_argument("name must not be empty");
            }
            if (uart_device_.empty()) {
                throw std::invalid_argument("uart_device must not be empty");
            }
            if (baud_ <= 0) {
                throw std::invalid_argument("baud must be > 0");
            }
            if (frame_id_.empty()) {
                throw std::invalid_argument("frame_id must not be empty");
            }
            if (status_topic_.empty()) {
                throw std::invalid_argument("status_topic must not be empty");
            }
            if (power_on_service_name_.empty() || power_off_service_name_.empty() ||
                    reset_service_name_.empty() || flight_mode_service_name_.empty() ||
                    prefer_rat_service_name_.empty() || set_pdp_service_name_.empty() ||
                    get_pdp_service_name_.empty() || data_call_service_name_.empty() ||
                    send_at_service_name_.empty())
            {
                throw std::invalid_argument("service names must not be empty");
            }
            if (status_period_ms_ <= 0) {
                throw std::invalid_argument("status_period_ms must be > 0");
            }
            if (default_cid_ < static_cast<int>(kMinCid) || default_cid_ > static_cast<int>(kMaxCid)) {
                throw std::invalid_argument("default_cid must be in [1, 20]");
            }
            if (default_pdp_type_ != static_cast<int>(MODEM_5G_PDP_IPV4) &&
                    default_pdp_type_ != static_cast<int>(MODEM_5G_PDP_IPV6) &&
                    default_pdp_type_ != static_cast<int>(MODEM_5G_PDP_IPV4V6))
            {
                throw std::invalid_argument("default_pdp_type must be one of: 1, 2, 3");
            }
            if (at_timeout_ms_ <= 0) {
                throw std::invalid_argument("at_timeout_ms must be > 0");
            }
            if (at_response_max_bytes_ < 2 || at_response_max_bytes_ > 16384) {
                throw std::invalid_argument("at_response_max_bytes must be in [2, 16384]");
            }
            if (!fits_fixed_field(default_apn_, MODEM_5G_APN_MAX_LEN)) {
                throw std::invalid_argument("default_apn is too long");
            }
            if (!fits_fixed_field(default_username_, MODEM_5G_USERNAME_MAX_LEN)) {
                throw std::invalid_argument("default_username is too long");
            }
            if (!fits_fixed_field(default_password_, MODEM_5G_PASSWORD_MAX_LEN)) {
                throw std::invalid_argument("default_password is too long");
            }
        }

        void cleanup_resources()
        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            if (dev_ == nullptr) {
                return;
            }

            const enum modem_5g_status deinit_rc = modem_5g_deinit(dev_);
            if (deinit_rc != MODEM_5G_STATUS_SUCCESS) {
                std::fprintf(stderr, "modem_5g_deinit failed: %s\n", describe_status(deinit_rc).c_str());
            }

            modem_5g_free(dev_);
            dev_ = nullptr;
        }

        enum modem_5g_status refresh_basic_info_locked()
        {
            struct modem_5g_basic_info info {};
            const enum modem_5g_status rc = modem_5g_get_basic_info(dev_, &info);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                cached_basic_info_ = info;
                basic_info_cached_ = true;
            }
            return rc;
        }

        enum modem_5g_status apply_default_pdp_context_locked()
        {
            struct modem_5g_pdp_context context {};
            context.cid = static_cast<uint8_t>(default_cid_);
            context.pdp_type = static_cast<enum modem_5g_pdp_type>(default_pdp_type_);
            copy_string_to_c_array(default_apn_, context.apn);
            copy_string_to_c_array(default_username_, context.username);
            copy_string_to_c_array(default_password_, context.password);

            const enum modem_5g_status rc = modem_5g_set_pdp_context(dev_, &context);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                active_cid_ = context.cid;
            }
            return rc;
        }

        uint8_t resolve_cid(uint8_t requested_cid) const
        {
            return requested_cid == 0 ? static_cast<uint8_t>(default_cid_) : requested_cid;
        }

        void note_snapshot_error(
            peripherals_5g_node::msg::Modem5gStatus * msg, enum modem_5g_status rc,
            const std::string & action) const
        {
            if (msg->snapshot_ok) {
                msg->snapshot_ok = false;
                msg->snapshot_status_code = static_cast<int32_t>(rc);
                msg->snapshot_message = action + ": " + describe_status(rc);
            }
        }

        peripherals_5g_node::msg::Modem5gStatus collect_status_snapshot_locked()
        {
            peripherals_5g_node::msg::Modem5gStatus msg;
            msg.header.stamp = now();
            msg.header.frame_id = frame_id_;
            msg.snapshot_ok = true;
            msg.snapshot_status_code = static_cast<int32_t>(MODEM_5G_STATUS_SUCCESS);
            msg.snapshot_message = describe_status(MODEM_5G_STATUS_SUCCESS);

            msg.device_name = name_;
            msg.uart_device = uart_device_;
            msg.baud = static_cast<uint32_t>(baud_);
            msg.flight_mode_enabled = flight_mode_enabled_;

            if (!basic_info_cached_) {
                const enum modem_5g_status rc = refresh_basic_info_locked();
                if (rc != MODEM_5G_STATUS_SUCCESS) {
                    note_snapshot_error(&msg, rc, "get_basic_info");
                }
            }

            if (basic_info_cached_) {
                msg.manufacturer = cached_basic_info_.manufacturer;
                msg.model = cached_basic_info_.model;
                msg.revision = cached_basic_info_.revision;
                msg.imei = cached_basic_info_.imei;
            }

            enum modem_5g_power_state power_state = MODEM_5G_POWER_OFF;
            enum modem_5g_status rc = modem_5g_get_power_state(dev_, &power_state);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.power_state = to_ros_power_state(power_state);
            } else {
                note_snapshot_error(&msg, rc, "get_power_state");
            }

            struct modem_5g_sim_info sim_info {};
            rc = modem_5g_get_sim_info(dev_, &sim_info);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.sim_state = to_ros_sim_state(sim_info.state);
                msg.iccid = sim_info.iccid;
                msg.imsi = sim_info.imsi;
                msg.msisdn = sim_info.msisdn;
            } else {
                note_snapshot_error(&msg, rc, "get_sim_info");
            }

            struct modem_5g_reg_info reg_info {};
            rc = modem_5g_get_reg_info(dev_, &reg_info);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.reg_state = to_ros_reg_state(reg_info.state);
                msg.rat = to_ros_rat(reg_info.rat);
                msg.mcc = reg_info.mcc;
                msg.mnc = reg_info.mnc;
                msg.tac = reg_info.tac;
                msg.cell_id = reg_info.cell_id;
                msg.pci = reg_info.pci;
                msg.arfcn = reg_info.arfcn;
                msg.band = reg_info.band;
                msg.operator_name = reg_info.operator_name;
            } else {
                note_snapshot_error(&msg, rc, "get_reg_info");
            }

            struct modem_5g_signal_info signal_info {};
            rc = modem_5g_get_signal_info(dev_, &signal_info);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.rssi_dbm = signal_info.rssi;
                msg.rsrp_dbm = signal_info.rsrp;
                msg.rsrq_db = signal_info.rsrq;
                msg.sinr_db = signal_info.sinr;
            } else {
                note_snapshot_error(&msg, rc, "get_signal_info");
            }

            enum modem_5g_data_state data_state = MODEM_5G_DATA_DISCONNECTED;
            rc = modem_5g_get_data_state(dev_, &data_state);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.data_state = to_ros_data_state(data_state);
            } else {
                note_snapshot_error(&msg, rc, "get_data_state");
            }

            msg.active_cid = active_cid_;
            const uint8_t cid = resolve_cid(active_cid_);
            msg.active_cid = cid;

            struct modem_5g_pdp_context pdp_context {};
            rc = modem_5g_get_pdp_context(dev_, cid, &pdp_context);
            if (rc == MODEM_5G_STATUS_SUCCESS) {
                msg.pdp_context = to_ros_pdp_context(pdp_context);
            }

            if (msg.data_state == to_ros_data_state(MODEM_5G_DATA_CONNECTED)) {
                struct modem_5g_ip_info ip_info {};
                rc = modem_5g_get_ip_info(dev_, cid, &ip_info);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    msg.ip_info = to_ros_ip_info(ip_info);
                }
            }

            return msg;
        }

        void publish_status_snapshot()
        {
            peripherals_5g_node::msg::Modem5gStatus msg;
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                if (dev_ == nullptr) {
                    return;
                }
                msg = collect_status_snapshot_locked();
            }
            status_pub_->publish(msg);
        }

        void on_power_on(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Request> /*request*/,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Response> response)
        {
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_power_on(dev_);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    flight_mode_enabled_ = false;
                }
                set_service_result(response.get(), rc, "power_on");
            }
            publish_status_snapshot();
        }

        void on_power_off(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Request> /*request*/,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Response> response)
        {
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_power_off(dev_);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    flight_mode_enabled_ = false;
                }
                set_service_result(response.get(), rc, "power_off");
            }
            publish_status_snapshot();
        }

        void on_reset(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Request> /*request*/,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gTrigger::Response> response)
        {
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_reset(dev_);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    flight_mode_enabled_ = false;
                }
                set_service_result(response.get(), rc, "reset");
            }
            publish_status_snapshot();
        }

        void on_set_flight_mode(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gSetFlightMode::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gSetFlightMode::Response> response)
        {
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_set_flight_mode(dev_, request->enable);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    flight_mode_enabled_ = request->enable;
                }
                set_service_result(response.get(), rc, "set_flight_mode");
                response->flight_mode_enabled =
                    (rc == MODEM_5G_STATUS_SUCCESS) ? request->enable : flight_mode_enabled_;
            }
            publish_status_snapshot();
        }

        void on_set_prefer_rat(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gSetPreferRat::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gSetPreferRat::Response> response)
        {
            const enum modem_5g_rat rat = rat_from_request(request->rat);
            if (rat == MODEM_5G_RAT_UNKNOWN) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "set_prefer_rat");
                response->rat = 0;
                return;
            }

            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_set_prefer_rat(dev_, rat);
                set_service_result(response.get(), rc, "set_prefer_rat");
                response->rat = request->rat;
            }
            publish_status_snapshot();
        }

        void on_set_pdp_context(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gSetPdpContext::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gSetPdpContext::Response> response)
        {
            const uint8_t cid = resolve_cid(request->cid);
            if (cid < kMinCid || cid > kMaxCid) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "set_pdp_context");
                return;
            }

            const uint8_t requested_pdp_type =
                request->pdp_type == 0 ? static_cast<uint8_t>(default_pdp_type_) : request->pdp_type;
            const enum modem_5g_pdp_type pdp_type = pdp_type_from_request(requested_pdp_type);
            if (pdp_type == MODEM_5G_PDP_UNKNOWN) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "set_pdp_context");
                return;
            }

            const std::string apn = request->apn.empty() ? default_apn_ : request->apn;
            const std::string username = request->username.empty() ? default_username_ : request->username;
            const std::string password = request->password.empty() ? default_password_ : request->password;
            if (!fits_fixed_field(apn, MODEM_5G_APN_MAX_LEN) ||
                    !fits_fixed_field(username, MODEM_5G_USERNAME_MAX_LEN) ||
                    !fits_fixed_field(password, MODEM_5G_PASSWORD_MAX_LEN))
            {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "set_pdp_context");
                response->message = "set_pdp_context: APN/username/password too long";
                return;
            }

            struct modem_5g_pdp_context context {};
            context.cid = cid;
            context.pdp_type = pdp_type;
            copy_string_to_c_array(apn, context.apn);
            copy_string_to_c_array(username, context.username);
            copy_string_to_c_array(password, context.password);

            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_set_pdp_context(dev_, &context);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    active_cid_ = cid;
                    response->context = to_ros_pdp_context(context);
                }
                set_service_result(response.get(), rc, "set_pdp_context");
            }
            publish_status_snapshot();
        }

        void on_get_pdp_context(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gGetPdpContext::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gGetPdpContext::Response> response)
        {
            const uint8_t cid = resolve_cid(request->cid);
            if (cid < kMinCid || cid > kMaxCid) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "get_pdp_context");
                return;
            }

            struct modem_5g_pdp_context context {};
            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_get_pdp_context(dev_, cid, &context);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    active_cid_ = cid;
                    response->context = to_ros_pdp_context(context);
                    response->password = context.password;
                }
                set_service_result(response.get(), rc, "get_pdp_context");
            }
            publish_status_snapshot();
        }

        void on_data_call(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gDataCall::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gDataCall::Response> response)
        {
            const uint8_t cid = resolve_cid(request->cid);
            if (cid < kMinCid || cid > kMaxCid) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "data_call");
                return;
            }

            enum modem_5g_data_state data_state =
                request->start ? MODEM_5G_DATA_CONNECTED : MODEM_5G_DATA_DISCONNECTED;

            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = request->start ?
                    modem_5g_data_start(dev_, cid) :
                    modem_5g_data_stop(dev_, cid);
                if (rc == MODEM_5G_STATUS_SUCCESS) {
                    active_cid_ = cid;
                    (void)modem_5g_get_data_state(dev_, &data_state);
                }
                set_service_result(response.get(), rc, request->start ? "data_start" : "data_stop");
                response->cid = cid;
                response->data_state = to_ros_data_state(data_state);
            }
            publish_status_snapshot();
        }

        void on_send_at(
                const std::shared_ptr<peripherals_5g_node::srv::Modem5gSendAt::Request> request,
                std::shared_ptr<peripherals_5g_node::srv::Modem5gSendAt::Response> response)
        {
            if (request->command.empty()) {
                set_service_result(response.get(), MODEM_5G_STATUS_INVALID, "send_at");
                return;
            }

            const uint32_t timeout_ms =
                request->timeout_ms == 0 ? static_cast<uint32_t>(at_timeout_ms_) : request->timeout_ms;
            std::vector<char> buffer(static_cast<size_t>(at_response_max_bytes_), '\0');

            {
                std::lock_guard<std::mutex> lock(api_mutex_);
                const enum modem_5g_status rc = modem_5g_send_at(
                        dev_, request->command.c_str(), buffer.data(), buffer.size(), timeout_ms);
                set_service_result(response.get(), rc, "send_at");
                response->response = buffer.data();
            }
        }

        std::string name_;
        std::string uart_device_;
        int baud_{9600};
        std::string frame_id_;

        std::string status_topic_;
        std::string power_on_service_name_;
        std::string power_off_service_name_;
        std::string reset_service_name_;
        std::string flight_mode_service_name_;
        std::string prefer_rat_service_name_;
        std::string set_pdp_service_name_;
        std::string get_pdp_service_name_;
        std::string data_call_service_name_;
        std::string send_at_service_name_;

        bool publish_on_startup_{true};
        int status_period_ms_{kDefaultStatusPeriodMs};

        int default_cid_{1};
        int default_pdp_type_{static_cast<int>(MODEM_5G_PDP_IPV4V6)};
        std::string default_apn_;
        std::string default_username_;
        std::string default_password_;

        bool power_on_on_startup_{false};
        bool apply_default_pdp_on_startup_{false};
        bool start_data_on_startup_{false};

        int at_timeout_ms_{static_cast<int>(kDefaultAtTimeoutMs)};
        int at_response_max_bytes_{kDefaultAtResponseMaxBytes};

        bool flight_mode_enabled_{false};
        uint8_t active_cid_{1};
        bool basic_info_cached_{false};
        struct modem_5g_basic_info cached_basic_info_ {};

        struct modem_5g_dev * dev_{nullptr};
        std::mutex api_mutex_;

        rclcpp::Publisher<peripherals_5g_node::msg::Modem5gStatus>::SharedPtr status_pub_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gTrigger>::SharedPtr power_on_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gTrigger>::SharedPtr power_off_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gTrigger>::SharedPtr reset_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gSetFlightMode>::SharedPtr flight_mode_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gSetPreferRat>::SharedPtr prefer_rat_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gSetPdpContext>::SharedPtr set_pdp_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gGetPdpContext>::SharedPtr get_pdp_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gDataCall>::SharedPtr data_call_srv_;
        rclcpp::Service<peripherals_5g_node::srv::Modem5gSendAt>::SharedPtr send_at_srv_;
        rclcpp::TimerBase::SharedPtr status_timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<Modem5gNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
