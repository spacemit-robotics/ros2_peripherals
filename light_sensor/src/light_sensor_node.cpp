#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/illuminance.hpp>

extern "C" {
#include <light_sensor.h>
}

class LightSensorNode final : public rclcpp::Node
{
public:
  LightSensorNode()
  : rclcpp::Node("light_sensor_node")
  {
    driver_ = declare_parameter<std::string>("driver", "W1160");
    name_ = declare_parameter<std::string>("name", "als0");
    i2c_dev_ = declare_parameter<std::string>("device", "/dev/i2c-5");
    i2c_addr_ = declare_parameter<int>("i2c_addr", 0x48);

    frame_id_ = declare_parameter<std::string>("frame_id", "light_sensor");
    topic_name_ = declare_parameter<std::string>("topic_name", "light_sensor/lux");
    poll_hz_ = declare_parameter<double>("poll_hz", 5.0);
    variance_ = declare_parameter<double>("variance", 0.0);

    validate_config();

    dev_ = light_sensor_alloc_i2c(compose_driver_instance().c_str(), i2c_dev_.c_str(),
      static_cast<uint8_t>(i2c_addr_));
    if (dev_ == nullptr) {
      throw std::runtime_error("light_sensor_alloc_i2c failed");
    }
    if (light_sensor_init(dev_) != 0) {
      light_sensor_free(dev_);
      dev_ = nullptr;
      throw std::runtime_error("light_sensor_init failed");
    }

    pub_ = create_publisher<sensor_msgs::msg::Illuminance>(topic_name_, rclcpp::SensorDataQoS());

    const auto period = std::chrono::duration<double>(1.0 / poll_hz_);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&LightSensorNode::tick, this));

    RCLCPP_INFO(
      get_logger(),
      "light_sensor_node ready: driver=%s name=%s device=%s addr=0x%X topic=%s poll_hz=%.3f",
      driver_.c_str(), name_.c_str(), i2c_dev_.c_str(), i2c_addr_, topic_name_.c_str(), poll_hz_);
  }

  ~LightSensorNode() override
  {
    if (dev_ != nullptr) {
      light_sensor_free(dev_);
      dev_ = nullptr;
    }
  }

private:
  void validate_config() const
  {
    if (driver_.empty()) {
      throw std::invalid_argument("driver must not be empty");
    }
    if (name_.empty()) {
      throw std::invalid_argument("name must not be empty");
    }
    if (i2c_dev_.empty()) {
      throw std::invalid_argument("device must not be empty");
    }
    if (i2c_addr_ < 0 || i2c_addr_ > 0x7f) {
      throw std::invalid_argument("i2c_addr must be in [0, 127]");
    }
    if (topic_name_.empty()) {
      throw std::invalid_argument("topic_name must not be empty");
    }
    if (!(poll_hz_ > 0.0)) {
      throw std::invalid_argument("poll_hz must be > 0");
    }
    if (std::isnan(variance_) || variance_ < 0.0) {
      throw std::invalid_argument("variance must be >= 0");
    }
  }

  std::string compose_driver_instance() const
  {
    return driver_ + ":" + name_;
  }

  void tick()
  {
    if (dev_ == nullptr) {
      return;
    }

    uint32_t lux = 0;
    const int rc = light_sensor_poll(dev_, &lux);
    if (rc == -EAGAIN) {
      return;
    }
    if (rc != 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "light_sensor_poll failed rc=%d", rc);
      return;
    }

    sensor_msgs::msg::Illuminance msg;
    msg.header.stamp = now();
    msg.header.frame_id = frame_id_;
    msg.illuminance = static_cast<double>(lux);
    msg.variance = variance_;
    pub_->publish(msg);
  }

  std::string driver_;
  std::string name_;
  std::string i2c_dev_;
  int i2c_addr_{0x48};

  std::string frame_id_;
  std::string topic_name_;
  double poll_hz_{5.0};
  double variance_{0.0};

  light_sensor_dev * dev_{nullptr};
  rclcpp::Publisher<sensor_msgs::msg::Illuminance>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<LightSensorNode>());
  } catch (const std::exception & e) {
    std::fprintf(stderr, "light_sensor_node exception: %s\n", e.what());
  }
  rclcpp::shutdown();
  return 0;
}
