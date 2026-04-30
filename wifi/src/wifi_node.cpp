extern "C" {
#include <wifi.h>
}

#include <algorithm>
#include <array>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <peripherals_wifi_node/msg/wifi_scan_result.hpp>
#include <peripherals_wifi_node/msg/wifi_scan_results.hpp>
#include <peripherals_wifi_node/msg/wifi_status.hpp>
#include <peripherals_wifi_node/srv/wifi_connect.hpp>
#include <peripherals_wifi_node/srv/wifi_scan.hpp>

namespace
{
constexpr uint32_t kMaxScanResultsUpperBound = 256;

std::string describe_status(enum wifi_status status)
{
    switch (status) {
        case WIFI_STATUS_SUCCESS:
            return "success";
        case WIFI_STATUS_FAIL:
            return "fail";
        case WIFI_STATUS_NOT_READY:
            return "not ready";
        case WIFI_STATUS_NOMEM:
            return "no memory";
        case WIFI_STATUS_BUSY:
            return "busy";
        case WIFI_STATUS_UNSUPPORTED:
            return "unsupported";
        case WIFI_STATUS_INVALID:
            return "invalid";
        case WIFI_STATUS_TIMEOUT:
            return "timeout";
        case WIFI_STATUS_UNHANDLED:
            return "unhandled";
        default:
            return "unknown(" + std::to_string(static_cast<int>(status)) + ")";
    }
}

bool is_sta_connected(enum wifi_sta_state state)
{
    return (
        state == WIFI_STA_CONNECTED ||
        state == WIFI_STA_OBTAINING_IP ||
        state == WIFI_STA_NET_CONNECTED);
}

uint8_t effective_mode(const struct wifi_state & state)
{
    if (state.current_mode == WIFI_MODE_STATION ||
            state.current_mode == WIFI_MODE_AP ||
            state.current_mode == WIFI_MODE_STATION_AP)
    {
        return static_cast<uint8_t>(state.current_mode);
    }

    if (state.ap_state != WIFI_AP_STATE_DISABLE) {
        return static_cast<uint8_t>(WIFI_MODE_AP);
    }
    if (state.current_mode_enable_flag != 0) {
        return static_cast<uint8_t>(WIFI_MODE_STATION);
    }
    return static_cast<uint8_t>(WIFI_MODE_UNKNOWN);
}

template<size_t N>
void copy_fixed_bytes(const uint8_t (&src)[N], std::array<uint8_t, N> * dst)
{
    std::copy(std::begin(src), std::end(src), dst->begin());
}

peripherals_wifi_node::msg::WifiScanResult to_ros_scan_result(const struct wifi_scan_result & result)
{
    peripherals_wifi_node::msg::WifiScanResult msg;
    copy_fixed_bytes(result.bssid, &msg.bssid);
    msg.ssid = result.ssid;
    msg.freq = result.freq;
    msg.rssi_dbm = result.rssi;
    msg.secure = static_cast<uint32_t>(result.key_mgmt);
    return msg;
}
}  // namespace

class WifiNode final : public rclcpp::Node
{
public:
    WifiNode()
    : rclcpp::Node("wifi_node")
    {
        frame_id_ = declare_parameter<std::string>("frame_id", "wifi");
        status_topic_ = declare_parameter<std::string>("status_topic", "wifi/status");
        scan_topic_ = declare_parameter<std::string>("scan_topic", "wifi/scan_results");
        connect_service_name_ = declare_parameter<std::string>("connect_service", "wifi/connect");
        disconnect_service_name_ = declare_parameter<std::string>("disconnect_service", "wifi/disconnect");
        scan_service_name_ = declare_parameter<std::string>("scan_service", "wifi/scan");

        enable_on_startup_ = declare_parameter<bool>("enable_on_startup", true);
        auto_reconnect_ = declare_parameter<bool>("auto_reconnect", true);
        status_period_ms_ = declare_parameter<int>("status_period_ms", 2000);
        scan_period_ms_ = declare_parameter<int>("scan_period_ms", 0);
        scan_on_startup_ = declare_parameter<bool>("scan_on_startup", false);
        scan_max_results_ = declare_parameter<int>("scan_max_results", 32);
        scan_ssid_ = declare_parameter<std::string>("scan_ssid", "");

        validate_config();

        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            const enum wifi_status init_rc = wifi_init();
            if (init_rc != WIFI_STATUS_SUCCESS) {
                throw std::runtime_error("wifi_init failed: " + describe_status(init_rc));
            }
        }

        status_pub_ = create_publisher<peripherals_wifi_node::msg::WifiStatus>(
            status_topic_, rclcpp::QoS(1).reliable().transient_local());
        scan_pub_ = create_publisher<peripherals_wifi_node::msg::WifiScanResults>(
            scan_topic_, rclcpp::QoS(1).reliable().transient_local());

        connect_srv_ = create_service<peripherals_wifi_node::srv::WifiConnect>(
            connect_service_name_,
            std::bind(&WifiNode::on_connect, this, std::placeholders::_1, std::placeholders::_2));
        disconnect_srv_ = create_service<std_srvs::srv::Trigger>(
            disconnect_service_name_,
            std::bind(&WifiNode::on_disconnect, this, std::placeholders::_1, std::placeholders::_2));
        scan_srv_ = create_service<peripherals_wifi_node::srv::WifiScan>(
            scan_service_name_,
            std::bind(&WifiNode::on_scan, this, std::placeholders::_1, std::placeholders::_2));

        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            if (enable_on_startup_) {
                const enum wifi_status on_rc = ensure_station_enabled_locked();
                if (on_rc != WIFI_STATUS_SUCCESS) {
                    throw std::runtime_error("wifi_on failed: " + describe_status(on_rc));
                }
            }

            if (auto_reconnect_) {
                const enum wifi_status auto_rc = wifi_sta_auto_reconnect(true);
                if (auto_rc != WIFI_STATUS_SUCCESS && auto_rc != WIFI_STATUS_NOT_READY) {
                    RCLCPP_WARN(
                        get_logger(), "wifi_sta_auto_reconnect(true) failed: %s",
                        describe_status(auto_rc).c_str());
                }
            }

            publish_status_snapshot_locked();

            if (scan_on_startup_) {
                std::vector<peripherals_wifi_node::msg::WifiScanResult> startup_results;
                uint32_t total_results = 0;
                std::string message;
                const bool ok = perform_scan_locked(
                    resolve_scan_filter(""), static_cast<uint32_t>(scan_max_results_),
                    &startup_results, &total_results, &message);
                if (!ok) {
                    RCLCPP_WARN(get_logger(), "startup scan failed: %s", message.c_str());
                }
            }
        }

        status_timer_ = create_wall_timer(
            std::chrono::milliseconds(status_period_ms_),
            std::bind(&WifiNode::publish_status_snapshot, this));

        if (scan_period_ms_ > 0) {
            scan_timer_ = create_wall_timer(
                std::chrono::milliseconds(scan_period_ms_),
                std::bind(&WifiNode::publish_periodic_scan, this));
        }

        RCLCPP_INFO(
            get_logger(),
            "wifi_node ready: status_topic=%s scan_topic=%s connect_service=%s "
            "disconnect_service=%s scan_service=%s status_period_ms=%d "
            "scan_period_ms=%d",
            status_topic_.c_str(), scan_topic_.c_str(), connect_service_name_.c_str(),
            disconnect_service_name_.c_str(), scan_service_name_.c_str(), status_period_ms_,
            scan_period_ms_);
    }

    ~WifiNode() override
    {
        std::lock_guard<std::mutex> lock(api_mutex_);
        const enum wifi_status rc = wifi_deinit();
        if (rc != WIFI_STATUS_SUCCESS) {
            std::fprintf(stderr, "wifi_deinit failed: %s\n", describe_status(rc).c_str());
        }
    }

private:
    void validate_config() const
    {
        if (frame_id_.empty()) {
            throw std::invalid_argument("frame_id must not be empty");
        }
        if (status_topic_.empty()) {
            throw std::invalid_argument("status_topic must not be empty");
        }
        if (scan_topic_.empty()) {
            throw std::invalid_argument("scan_topic must not be empty");
        }
        if (connect_service_name_.empty()) {
            throw std::invalid_argument("connect_service must not be empty");
        }
        if (disconnect_service_name_.empty()) {
            throw std::invalid_argument("disconnect_service must not be empty");
        }
        if (scan_service_name_.empty()) {
            throw std::invalid_argument("scan_service must not be empty");
        }
        if (status_period_ms_ <= 0) {
            throw std::invalid_argument("status_period_ms must be > 0");
        }
        if (scan_period_ms_ < 0) {
            throw std::invalid_argument("scan_period_ms must be >= 0");
        }
        if (scan_max_results_ <= 0 || scan_max_results_ > static_cast<int>(kMaxScanResultsUpperBound)) {
            throw std::invalid_argument("scan_max_results must be in [1, 256]");
        }
    }

    enum wifi_status ensure_station_enabled_locked()
    {
        struct wifi_state state {};
        const enum wifi_status state_rc = wifi_get_state(&state);
        if (state_rc == WIFI_STATUS_SUCCESS && state.current_mode_enable_flag != 0) {
            return WIFI_STATUS_SUCCESS;
        }
        return wifi_on(WIFI_MODE_STATION);
    }

    std::string resolve_scan_filter(const std::string & requested_ssid) const
    {
        if (!requested_ssid.empty()) {
            return requested_ssid;
        }
        return scan_ssid_;
    }

    uint32_t effective_scan_limit(uint32_t requested_limit) const
    {
        if (requested_limit == 0) {
            return static_cast<uint32_t>(scan_max_results_);
        }
        return std::min<uint32_t>(requested_limit, kMaxScanResultsUpperBound);
    }

    peripherals_wifi_node::msg::WifiStatus build_status_msg_locked()
    {
        peripherals_wifi_node::msg::WifiStatus msg;
        msg.header.stamp = now();
        msg.header.frame_id = frame_id_;
        msg.mode = static_cast<uint8_t>(WIFI_MODE_UNKNOWN);
        msg.connected = false;
        msg.ssid.clear();
        msg.rssi_dbm = 0;
        msg.secure = 0;
        msg.bssid.fill(0);
        msg.ip_addr.fill(0);

        struct wifi_state state {};
        const enum wifi_status state_rc = wifi_get_state(&state);
        if (state_rc != WIFI_STATUS_SUCCESS) {
            throw std::runtime_error("wifi_get_state failed: " + describe_status(state_rc));
        }

        msg.mode = effective_mode(state);
        msg.connected = is_sta_connected(state.sta_state);

        if (!msg.connected) {
            return msg;
        }

        struct wifi_sta_info info {};
        const enum wifi_status info_rc = wifi_sta_get_info(&info);
        if (info_rc != WIFI_STATUS_SUCCESS) {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), 3000,
                "wifi_sta_get_info failed while connection is active: %s",
                describe_status(info_rc).c_str());
            return msg;
        }

        msg.ssid = info.ssid;
        msg.rssi_dbm = info.rssi;
        msg.secure = static_cast<uint32_t>(info.sec);
        copy_fixed_bytes(info.bssid, &msg.bssid);
        copy_fixed_bytes(info.ip_addr, &msg.ip_addr);
        return msg;
    }

    void publish_status_snapshot_locked()
    {
        try {
            status_pub_->publish(build_status_msg_locked());
        } catch (const std::exception & e) {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), 3000, "failed to publish wifi status: %s", e.what());
        }
    }

    void publish_status_snapshot()
    {
        std::lock_guard<std::mutex> lock(api_mutex_);
        publish_status_snapshot_locked();
    }

    void publish_scan_results_locked(const std::vector<peripherals_wifi_node::msg::WifiScanResult> & results)
    {
        peripherals_wifi_node::msg::WifiScanResults msg;
        msg.header.stamp = now();
        msg.header.frame_id = frame_id_;
        msg.results = results;
        scan_pub_->publish(msg);
    }

    bool perform_scan_locked(
        const std::string & filter_ssid, uint32_t requested_limit,
        std::vector<peripherals_wifi_node::msg::WifiScanResult> * results_out, uint32_t * total_results_out,
        std::string * message_out)
    {
        const uint32_t limit = effective_scan_limit(requested_limit);
        const enum wifi_status on_rc = ensure_station_enabled_locked();
        if (on_rc != WIFI_STATUS_SUCCESS) {
            if (message_out != nullptr) {
                *message_out = "wifi_on failed: " + describe_status(on_rc);
            }
            return false;
        }

        std::vector<struct wifi_scan_result> raw_results(limit);
        uint32_t total_results = 0;
        const char * filter = filter_ssid.empty() ? nullptr : filter_ssid.c_str();
        const enum wifi_status scan_rc = wifi_get_scan_results(
            raw_results.data(), filter, &total_results, limit);
        if (scan_rc != WIFI_STATUS_SUCCESS) {
            if (message_out != nullptr) {
                *message_out = "scan failed: " + describe_status(scan_rc);
            }
            return false;
        }

        const uint32_t stored_results = std::min<uint32_t>(total_results, limit);
        std::vector<peripherals_wifi_node::msg::WifiScanResult> ros_results;
        ros_results.reserve(stored_results);
        for (uint32_t i = 0; i < stored_results; ++i) {
            ros_results.push_back(to_ros_scan_result(raw_results[i]));
        }

        if (results_out != nullptr) {
            *results_out = ros_results;
        }
        if (total_results_out != nullptr) {
            *total_results_out = total_results;
        }

        publish_scan_results_locked(ros_results);

        if (message_out != nullptr) {
            if (requested_limit > kMaxScanResultsUpperBound) {
                *message_out = "ok (max_results clamped to 256)";
            } else if (total_results > stored_results) {
                *message_out = "ok (results truncated by max_results)";
            } else {
                *message_out = "ok";
            }
        }
        return true;
    }

    void publish_periodic_scan()
    {
        std::lock_guard<std::mutex> lock(api_mutex_);

        std::vector<peripherals_wifi_node::msg::WifiScanResult> results;
        uint32_t total_results = 0;
        std::string message;
        const bool ok = perform_scan_locked(
            resolve_scan_filter(""), static_cast<uint32_t>(scan_max_results_),
            &results, &total_results, &message);
        if (!ok) {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), 5000, "periodic wifi scan failed: %s", message.c_str());
        }
    }

    void on_connect(
        const std::shared_ptr<peripherals_wifi_node::srv::WifiConnect::Request> req,
        std::shared_ptr<peripherals_wifi_node::srv::WifiConnect::Response> resp)
    {
        if (req->ssid.empty()) {
            resp->success = false;
            resp->message = "ssid must not be empty";
            return;
        }

        std::lock_guard<std::mutex> lock(api_mutex_);

        const enum wifi_status on_rc = ensure_station_enabled_locked();
        if (on_rc != WIFI_STATUS_SUCCESS) {
            resp->success = false;
            resp->message = "wifi_on failed: " + describe_status(on_rc);
            publish_status_snapshot_locked();
            return;
        }

        struct wifi_sta_connect_param param {};
        param.ssid = req->ssid.c_str();
        param.password = req->password.empty() ? nullptr : req->password.c_str();
        param.sec = WIFI_SEC_UNKNOWN;
        param.fast_connect = false;
        std::copy(req->bssid.begin(), req->bssid.end(), param.bssid);

        const enum wifi_status rc = wifi_sta_connect(&param);
        resp->success = (rc == WIFI_STATUS_SUCCESS);
        resp->message = resp->success ? "ok" : "connect failed: " + describe_status(rc);

        publish_status_snapshot_locked();
    }

    void on_disconnect(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> /*req*/,
        std::shared_ptr<std_srvs::srv::Trigger::Response> resp)
    {
        std::lock_guard<std::mutex> lock(api_mutex_);

        const enum wifi_status rc = wifi_sta_disconnect();
        if (rc == WIFI_STATUS_SUCCESS) {
            resp->success = true;
            resp->message = "ok";
        } else if (rc == WIFI_STATUS_NOT_READY) {
            resp->success = true;
            resp->message = "already disconnected";
        } else {
            resp->success = false;
            resp->message = "disconnect failed: " + describe_status(rc);
        }

        publish_status_snapshot_locked();
    }

    void on_scan(
        const std::shared_ptr<peripherals_wifi_node::srv::WifiScan::Request> req,
        std::shared_ptr<peripherals_wifi_node::srv::WifiScan::Response> resp)
    {
        std::lock_guard<std::mutex> lock(api_mutex_);

        const std::string filter_ssid = resolve_scan_filter(req->ssid);
        const bool ok = perform_scan_locked(
            filter_ssid, req->max_results, &resp->results, &resp->total_results, &resp->message);
        resp->success = ok;
    }

    std::string frame_id_;
    std::string status_topic_;
    std::string scan_topic_;
    std::string connect_service_name_;
    std::string disconnect_service_name_;
    std::string scan_service_name_;
    bool enable_on_startup_{true};
    bool auto_reconnect_{true};
    int status_period_ms_{2000};
    int scan_period_ms_{0};
    bool scan_on_startup_{false};
    int scan_max_results_{32};
    std::string scan_ssid_;

    std::mutex api_mutex_;

    rclcpp::Publisher<peripherals_wifi_node::msg::WifiStatus>::SharedPtr status_pub_;
    rclcpp::Publisher<peripherals_wifi_node::msg::WifiScanResults>::SharedPtr scan_pub_;
    rclcpp::Service<peripherals_wifi_node::srv::WifiConnect>::SharedPtr connect_srv_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr disconnect_srv_;
    rclcpp::Service<peripherals_wifi_node::srv::WifiScan>::SharedPtr scan_srv_;
    rclcpp::TimerBase::SharedPtr status_timer_;
    rclcpp::TimerBase::SharedPtr scan_timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<WifiNode>());
    } catch (const std::exception & e) {
        std::fprintf(stderr, "wifi_node exception: %s\n", e.what());
    }
    rclcpp::shutdown();
    return 0;
}
