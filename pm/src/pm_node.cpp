#include <pm.h>

#include <rclcpp/rclcpp.hpp>

#include <peripherals_pm_node/msg/power_status.hpp>
#include <peripherals_pm_node/srv/set_power_switch.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr size_t kMaxCellCount = 16;

std::string normalize_driver(std::string driver)
{
  std::transform(driver.begin(), driver.end(), driver.begin(), [](unsigned char ch) {
    if (ch == '_') {
      return static_cast<char>('-');
    }
    return static_cast<char>(std::tolower(ch));
  });

  if (driver == "generic") {
    return "generic";
  }
  if (driver == "adc" || driver == "adc-linux") {
    return "adc";
  }
  if (driver == "ina219" || driver == "i2c-ina219") {
    return "ina219";
  }
  return driver;
}

std::string describe_rc(int rc)
{
  if (rc >= 0) {
    return "ok";
  }

  const int err = -rc;
  const char * text = std::strerror(err);
  if (text == nullptr || *text == '\0') {
    return "rc=" + std::to_string(rc);
  }
  return "rc=" + std::to_string(rc) + " (" + text + ")";
}

uint8_t to_ros_status(enum pm_status status)
{
  switch (status) {
    case PM_STATUS_DISCHARGING:
      return 1;
    case PM_STATUS_CHARGING:
      return 2;
    case PM_STATUS_FULL:
      return 3;
    case PM_STATUS_FAULT:
      return 4;
    case PM_STATUS_SLEEP:
      return 5;
    case PM_STATUS_UNKNOWN:
    default:
      return 0;
  }
}
}  // namespace

class PmNode final : public rclcpp::Node
{
public:
  PmNode()
  : rclcpp::Node("pm_node")
  {
    try {
      driver_ = normalize_driver(declare_parameter<std::string>("driver", "generic"));
      instance_name_ = declare_parameter<std::string>("name", "main_batt");

      frame_id_ = declare_parameter<std::string>("frame_id", "power");
      status_topic_ = declare_parameter<std::string>("status_topic", "power/status");
      switch_service_name_ = declare_parameter<std::string>("switch_service", "power/set_switch");
      poll_hz_ = declare_parameter<double>("poll_hz", 1.0);
      publish_on_startup_ = declare_parameter<bool>("publish_on_startup", true);

      charger_node_ = declare_parameter<std::string>("charger_node", "ip2317-charger");
      capacity_node_ = declare_parameter<std::string>("capacity_node", "cw-bat");
      device_ = declare_parameter<std::string>("device", "/dev/i2c-5");
      i2c_addr_ = declare_parameter<int>("i2c_addr", 0x40);
      adc_scale_ = declare_parameter<double>("adc_scale", 11.0);

      capacity_mah_ = declare_parameter<double>("capacity_mah", 5000.0);
      max_voltage_ = declare_parameter<double>("max_voltage", 12.6);
      min_voltage_ = declare_parameter<double>("min_voltage", 9.0);
      warn_voltage_ = declare_parameter<double>("warn_voltage", 10.0);
      crit_voltage_ = declare_parameter<double>("crit_voltage", 9.5);
      max_temp_ = declare_parameter<double>("max_temp", 60.0);

      switch_channels_ = declare_parameter<std::vector<std::string>>(
        "switch_channels", std::vector<std::string>{});

      validate_config();

      dev_ = alloc_device();
      if (dev_ == nullptr) {
        throw std::runtime_error("failed to allocate pm device for driver=" + driver_);
      }

      const auto config = build_pm_config();
      const int init_rc = pm_init(dev_, &config);
      if (init_rc != 0) {
        throw std::runtime_error("pm_init failed: " + describe_rc(init_rc));
      }

      const int start_rc = pm_start(dev_, timer_frequency_hz());
      if (start_rc != 0) {
        throw std::runtime_error("pm_start failed: " + describe_rc(start_rc));
      }

      status_pub_ = create_publisher<peripherals_pm_node::msg::PowerStatus>(
        status_topic_, rclcpp::QoS(1).reliable().transient_local());

      switch_srv_ = create_service<peripherals_pm_node::srv::SetPowerSwitch>(
        switch_service_name_,
        std::bind(&PmNode::on_set_power_switch, this, std::placeholders::_1, std::placeholders::_2));

      const auto period = std::chrono::duration<double>(1.0 / poll_hz_);
      timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&PmNode::publish_status_snapshot, this));

      if (publish_on_startup_) {
        publish_status_snapshot();
      }

      RCLCPP_INFO(
        get_logger(),
        "pm_node ready: driver=%s name=%s topic=%s switch_service=%s poll_hz=%.3f",
        driver_.c_str(), instance_name_.c_str(), status_pub_->get_topic_name(),
        switch_service_name_.c_str(), poll_hz_);
    } catch (...) {
      cleanup_resources();
      throw;
    }
  }

  ~PmNode() override
  {
    cleanup_resources();
  }

private:
  void validate_config() const
  {
    const auto finite_positive = [](double value) {
        return std::isfinite(value) && value > 0.0;
      };

    if (driver_ != "generic" && driver_ != "adc" && driver_ != "ina219") {
      throw std::invalid_argument("driver must be one of: generic, adc, ina219");
    }
    if (instance_name_.empty()) {
      throw std::invalid_argument("name must not be empty");
    }
    if (frame_id_.empty()) {
      throw std::invalid_argument("frame_id must not be empty");
    }
    if (status_topic_.empty()) {
      throw std::invalid_argument("status_topic must not be empty");
    }
    if (switch_service_name_.empty()) {
      throw std::invalid_argument("switch_service must not be empty");
    }
    if (!finite_positive(poll_hz_)) {
      throw std::invalid_argument("poll_hz must be > 0");
    }
    if (!finite_positive(capacity_mah_)) {
      throw std::invalid_argument("capacity_mah must be > 0");
    }
    if (!finite_positive(max_voltage_) || !finite_positive(min_voltage_)) {
      throw std::invalid_argument("max_voltage and min_voltage must be > 0");
    }
    if (!(max_voltage_ > min_voltage_)) {
      throw std::invalid_argument("max_voltage must be greater than min_voltage");
    }
    if (!std::isfinite(warn_voltage_) || !std::isfinite(crit_voltage_)) {
      throw std::invalid_argument("warn_voltage and crit_voltage must be finite");
    }
    if (crit_voltage_ < min_voltage_ || crit_voltage_ > max_voltage_) {
      throw std::invalid_argument("crit_voltage must be within [min_voltage, max_voltage]");
    }
    if (warn_voltage_ < min_voltage_ || warn_voltage_ > max_voltage_) {
      throw std::invalid_argument("warn_voltage must be within [min_voltage, max_voltage]");
    }
    if (warn_voltage_ < crit_voltage_) {
      throw std::invalid_argument("warn_voltage must be >= crit_voltage");
    }
    if (!finite_positive(max_temp_)) {
      throw std::invalid_argument("max_temp must be > 0");
    }
    if (switch_channels_.size() > 256) {
      throw std::invalid_argument("switch_channels must contain at most 256 entries");
    }

    if (driver_ == "generic") {
      if (charger_node_.empty()) {
        throw std::invalid_argument("charger_node must not be empty for generic driver");
      }
      if (capacity_node_.empty()) {
        throw std::invalid_argument("capacity_node must not be empty for generic driver");
      }
      return;
    }

    if (device_.empty()) {
      throw std::invalid_argument("device must not be empty for adc/ina219 drivers");
    }
    if (driver_ == "adc" && !finite_positive(adc_scale_)) {
      throw std::invalid_argument("adc_scale must be > 0 for adc driver");
    }
    if (driver_ == "ina219" && (i2c_addr_ < 0 || i2c_addr_ > 0x7f)) {
      throw std::invalid_argument("i2c_addr must be in [0, 127] for ina219 driver");
    }
  }

  struct pm_config build_pm_config() const
  {
    struct pm_config config{};
    config.capacity_mah = static_cast<float>(capacity_mah_);
    config.max_voltage = static_cast<float>(max_voltage_);
    config.min_voltage = static_cast<float>(min_voltage_);
    config.warn_voltage = static_cast<float>(warn_voltage_);
    config.crit_voltage = static_cast<float>(crit_voltage_);
    config.max_temp = static_cast<float>(max_temp_);
    return config;
  }

  uint32_t timer_frequency_hz() const
  {
    return static_cast<uint32_t>(std::max(1, static_cast<int>(std::ceil(poll_hz_))));
  }

  struct pm_dev * alloc_device() const
  {
    if (driver_ == "generic") {
      return pm_alloc_generic(instance_name_.c_str(), charger_node_.c_str(), capacity_node_.c_str(), nullptr);
    }
    if (driver_ == "adc") {
      return pm_alloc_adc(instance_name_.c_str(), device_.c_str(), static_cast<float>(adc_scale_));
    }
    return pm_alloc_digital(
      instance_name_.c_str(), "INA219", device_.c_str(), static_cast<uint32_t>(i2c_addr_));
  }

  peripherals_pm_node::msg::PowerStatus to_ros_msg(const struct pm_state & state) const
  {
    peripherals_pm_node::msg::PowerStatus msg;
    msg.header.stamp = now();
    msg.header.frame_id = frame_id_;
    msg.status = to_ros_status(state.status);
    msg.percentage = std::clamp(
      std::isfinite(state.percentage) ? state.percentage : 0.0f, 0.0f, 100.0f);
    msg.health = std::isfinite(state.health) ? state.health : 0.0f;
    msg.cycle_count = state.cycle_count;
    msg.error_code = state.error_code;

    const size_t cell_count = std::min(static_cast<size_t>(state.cell_count), kMaxCellCount);
    msg.cell_count = static_cast<uint8_t>(cell_count);
    msg.cell_voltages.assign(state.cell_voltages, state.cell_voltages + cell_count);
    return msg;
  }

  void publish_status_snapshot()
  {
    struct pm_state state{};
    int rc = 0;

    {
      std::lock_guard<std::mutex> lock(dev_mutex_);
      if (dev_ == nullptr) {
        return;
      }
      rc = pm_get_state(dev_, &state);
    }

    if (rc != 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000, "pm_get_state failed: %s", describe_rc(rc).c_str());
      return;
    }

    status_pub_->publish(to_ros_msg(state));
  }

  void on_set_power_switch(
    const std::shared_ptr<peripherals_pm_node::srv::SetPowerSwitch::Request> req,
    std::shared_ptr<peripherals_pm_node::srv::SetPowerSwitch::Response> resp)
  {
    const size_t channel_index = req->channel;
    if (channel_index >= switch_channels_.size() || switch_channels_[channel_index].empty()) {
      resp->success = false;
      resp->message = "channel " + std::to_string(channel_index) + " is not configured";
      return;
    }

    const std::string & channel_name = switch_channels_[channel_index];
    int rc = 0;
    {
      std::lock_guard<std::mutex> lock(dev_mutex_);
      if (dev_ == nullptr) {
        resp->success = false;
        resp->message = "pm device is not available";
        return;
      }
      rc = pm_switch_set(dev_, channel_name.c_str(), req->enable);
    }

    if (rc != 0) {
      resp->success = false;
      resp->message =
        "pm_switch_set failed for channel " + std::to_string(channel_index) + " (" + channel_name +
        "): " + describe_rc(rc);
      RCLCPP_WARN(get_logger(), "%s", resp->message.c_str());
      return;
    }

    resp->success = true;
    resp->message =
      "channel " + std::to_string(channel_index) + " (" + channel_name + ") set to " +
      (req->enable ? "on" : "off");
    RCLCPP_INFO(get_logger(), "%s", resp->message.c_str());
  }

  void cleanup_resources()
  {
    timer_.reset();
    switch_srv_.reset();
    status_pub_.reset();

    std::lock_guard<std::mutex> lock(dev_mutex_);
    if (dev_ != nullptr) {
      pm_free(dev_);
      dev_ = nullptr;
    }
  }

  std::string driver_;
  std::string instance_name_;

  std::string frame_id_;
  std::string status_topic_;
  std::string switch_service_name_;
  double poll_hz_{1.0};
  bool publish_on_startup_{true};

  std::string charger_node_;
  std::string capacity_node_;
  std::string device_;
  int i2c_addr_{0x40};
  double adc_scale_{11.0};

  double capacity_mah_{5000.0};
  double max_voltage_{12.6};
  double min_voltage_{9.0};
  double warn_voltage_{10.0};
  double crit_voltage_{9.5};
  double max_temp_{60.0};

  std::vector<std::string> switch_channels_;

  struct pm_dev * dev_{nullptr};
  std::mutex dev_mutex_;
  rclcpp::Publisher<peripherals_pm_node::msg::PowerStatus>::SharedPtr status_pub_;
  rclcpp::Service<peripherals_pm_node::srv::SetPowerSwitch>::SharedPtr switch_srv_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<PmNode>());
  } catch (const std::exception & e) {
    std::fprintf(stderr, "pm_node exception: %s\n", e.what());
  }
  rclcpp::shutdown();
  return 0;
}
