#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <peripherals_nfc_node/msg/nfc_tag_info.hpp>
#include <peripherals_nfc_node/srv/nfc_poll.hpp>
#include <peripherals_nfc_node/srv/nfc_read_block.hpp>
#include <peripherals_nfc_node/srv/nfc_write_block.hpp>

extern "C" {
#include <nfc.h>
}

namespace
{
constexpr size_t kUidMaxLen = 16;
constexpr size_t kAtsMaxLen = 32;
constexpr size_t kBlockLen = 16;

std::string describe_rc(int rc)
{
  if (rc >= 0) {
    return "ok";
  }

  std::ostringstream oss;
  oss << "rc=" << rc;
  const int err = -rc;
  const char * text = std::strerror(err);
  if (text != nullptr && std::strcmp(text, "Unknown error") != 0) {
    oss << " (" << text << ")";
  }
  return oss.str();
}
}  // namespace

class NfcNode final : public rclcpp::Node
{
public:
  NfcNode()
  : rclcpp::Node("nfc_node")
  {
    transport_ = declare_parameter<std::string>("transport", "i2c");
    driver_ = declare_parameter<std::string>("driver", "SI512");
    instance_name_ = declare_parameter<std::string>("name", "nfc0");
    device_ = declare_parameter<std::string>("device", "/dev/i2c-5");
    i2c_addr_ = declare_parameter<int>("i2c_addr", 0x28);
    cs_pin_ = declare_parameter<int>("cs_pin", 0);
    baud_ = declare_parameter<int>("baud", 115200);
    frame_id_ = declare_parameter<std::string>("frame_id", "nfc");

    tag_topic_ = declare_parameter<std::string>("tag_topic", "nfc/tag_detected");
    poll_service_name_ = declare_parameter<std::string>("poll_service", "nfc/poll");
    read_service_name_ = declare_parameter<std::string>("read_service", "nfc/read_block");
    write_service_name_ = declare_parameter<std::string>("write_service", "nfc/write_block");

    auto_poll_enabled_ = declare_parameter<bool>("auto_poll_enabled", true);
    poll_period_ms_ = declare_parameter<int>("poll_period_ms", 100);
    poll_timeout_ms_ = declare_parameter<int>("poll_timeout_ms", 50);
    publish_duplicates_ = declare_parameter<bool>("publish_duplicates", false);

    validate_config();

    dev_ = alloc_device();
    if (dev_ == nullptr) {
      throw std::runtime_error("nfc_alloc_* failed");
    }
    if (nfc_init(dev_) != 0) {
      nfc_free(dev_);
      dev_ = nullptr;
      throw std::runtime_error("nfc_init failed");
    }

    tag_pub_ = create_publisher<peripherals_nfc_node::msg::NfcTagInfo>(tag_topic_, rclcpp::QoS(10).reliable());

    poll_srv_ = create_service<peripherals_nfc_node::srv::NfcPoll>(
      poll_service_name_,
      std::bind(&NfcNode::on_poll, this, std::placeholders::_1, std::placeholders::_2));

    read_srv_ = create_service<peripherals_nfc_node::srv::NfcReadBlock>(
      read_service_name_,
      std::bind(&NfcNode::on_read, this, std::placeholders::_1, std::placeholders::_2));

    write_srv_ = create_service<peripherals_nfc_node::srv::NfcWriteBlock>(
      write_service_name_,
      std::bind(&NfcNode::on_write, this, std::placeholders::_1, std::placeholders::_2));

    if (auto_poll_enabled_) {
      poll_timer_ = create_wall_timer(
        std::chrono::milliseconds(poll_period_ms_),
        std::bind(&NfcNode::poll_once, this));
    }

    RCLCPP_INFO(
      get_logger(),
      "nfc_node ready: transport=%s driver=%s name=%s device=%s tag_topic=%s auto_poll=%s period_ms=%d timeout_ms=%d",
      transport_.c_str(), driver_.c_str(), instance_name_.c_str(), device_.c_str(), tag_topic_.c_str(),
      auto_poll_enabled_ ? "true" : "false", poll_period_ms_, poll_timeout_ms_);
  }

  ~NfcNode() override
  {
    std::lock_guard<std::mutex> lock(device_mutex_);
    if (dev_ != nullptr) {
      nfc_free(dev_);
      dev_ = nullptr;
    }
  }

private:
  void validate_config() const
  {
    if (transport_ != "i2c" && transport_ != "spi" && transport_ != "uart") {
      throw std::invalid_argument("transport must be one of: i2c, spi, uart");
    }
    if (driver_.empty()) {
      throw std::invalid_argument("driver must not be empty");
    }
    if (instance_name_.empty()) {
      throw std::invalid_argument("name must not be empty");
    }
    if (device_.empty()) {
      throw std::invalid_argument("device must not be empty");
    }
    if (poll_period_ms_ <= 0) {
      throw std::invalid_argument("poll_period_ms must be > 0");
    }
    if (poll_timeout_ms_ < 0) {
      throw std::invalid_argument("poll_timeout_ms must be >= 0");
    }
    if (i2c_addr_ < 0 || i2c_addr_ > 0x7f) {
      throw std::invalid_argument("i2c_addr must be in [0, 127]");
    }
    if (cs_pin_ < 0) {
      throw std::invalid_argument("cs_pin must be >= 0");
    }
    if (baud_ <= 0) {
      throw std::invalid_argument("baud must be > 0");
    }
  }

  std::string compose_device_name() const
  {
    return driver_ + ":" + instance_name_;
  }

  nfc_dev * alloc_device() const
  {
    const std::string full_name = compose_device_name();

    if (transport_ == "i2c") {
      return nfc_alloc_i2c(full_name.c_str(), device_.c_str(), static_cast<uint8_t>(i2c_addr_));
    }
    if (transport_ == "spi") {
      return nfc_alloc_spi(full_name.c_str(), device_.c_str(), static_cast<uint32_t>(cs_pin_));
    }
    return nfc_alloc_uart(full_name.c_str(), device_.c_str(), static_cast<uint32_t>(baud_));
  }

  peripherals_nfc_node::msg::NfcTagInfo to_ros_msg(const struct nfc_tag_info & info) const
  {
    peripherals_nfc_node::msg::NfcTagInfo msg;
    msg.header.stamp = now();
    msg.header.frame_id = frame_id_;

    const size_t uid_len = std::min(static_cast<size_t>(info.uid_len), kUidMaxLen);
    const size_t ats_len = std::min(static_cast<size_t>(info.ats_len), kAtsMaxLen);

    msg.uid_len = static_cast<uint8_t>(uid_len);
    msg.uid.assign(info.uid, info.uid + uid_len);
    msg.tag_type = static_cast<uint8_t>(info.type);
    msg.rssi_dbm = info.rssi_dbm;
    msg.ats_len = static_cast<uint8_t>(ats_len);
    msg.ats.assign(info.ats, info.ats + ats_len);
    return msg;
  }

  bool same_tag(const struct nfc_tag_info & lhs, const struct nfc_tag_info & rhs) const
  {
    const size_t lhs_uid_len = std::min(static_cast<size_t>(lhs.uid_len), kUidMaxLen);
    const size_t rhs_uid_len = std::min(static_cast<size_t>(rhs.uid_len), kUidMaxLen);
    const size_t lhs_ats_len = std::min(static_cast<size_t>(lhs.ats_len), kAtsMaxLen);
    const size_t rhs_ats_len = std::min(static_cast<size_t>(rhs.ats_len), kAtsMaxLen);

    if (lhs_uid_len != rhs_uid_len || lhs_ats_len != rhs_ats_len) {
      return false;
    }
    if (lhs.type != rhs.type || lhs.rssi_dbm != rhs.rssi_dbm) {
      return false;
    }
    if (!std::equal(lhs.uid, lhs.uid + lhs_uid_len, rhs.uid)) {
      return false;
    }
    if (!std::equal(lhs.ats, lhs.ats + lhs_ats_len, rhs.ats)) {
      return false;
    }
    return true;
  }

  void publish_detected_tag(const struct nfc_tag_info & info)
  {
    if (!publish_duplicates_ && has_last_tag_ && same_tag(info, last_tag_info_)) {
      return;
    }

    tag_pub_->publish(to_ros_msg(info));
    last_tag_info_ = info;
    has_last_tag_ = true;
  }

  void clear_last_tag()
  {
    has_last_tag_ = false;
    std::memset(&last_tag_info_, 0, sizeof(last_tag_info_));
  }

  void poll_once()
  {
    std::lock_guard<std::mutex> lock(device_mutex_);
    if (dev_ == nullptr) {
      return;
    }

    struct nfc_tag_info info {};
    const int rc = nfc_poll(dev_, &info, static_cast<uint32_t>(poll_timeout_ms_));
    if (rc == 0) {
      publish_detected_tag(info);
      return;
    }
    if (rc == 1) {
      clear_last_tag();
      return;
    }

    clear_last_tag();
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 3000, "nfc_poll failed during auto polling: %s",
      describe_rc(rc).c_str());
  }

  void on_poll(
    const std::shared_ptr<peripherals_nfc_node::srv::NfcPoll::Request> req,
    std::shared_ptr<peripherals_nfc_node::srv::NfcPoll::Response> resp)
  {
    std::lock_guard<std::mutex> lock(device_mutex_);

    struct nfc_tag_info info {};
    const int rc = nfc_poll(dev_, &info, req->timeout_ms);
    if (rc == 0) {
      resp->success = true;
      resp->message = "tag detected";
      resp->tag_info = to_ros_msg(info);
      publish_detected_tag(info);
      return;
    }

    resp->success = false;
    if (rc == 1) {
      resp->message = "no tag";
      clear_last_tag();
      return;
    }

    clear_last_tag();
    resp->message = "poll failed: " + describe_rc(rc);
  }

  void on_read(
    const std::shared_ptr<peripherals_nfc_node::srv::NfcReadBlock::Request> req,
    std::shared_ptr<peripherals_nfc_node::srv::NfcReadBlock::Response> resp)
  {
    std::lock_guard<std::mutex> lock(device_mutex_);

    std::array<uint8_t, kBlockLen> buf {};
    const int rc = nfc_read_block(dev_, req->block_addr, buf.data(), buf.size());
    if (rc > 0) {
      resp->success = true;
      resp->message = "ok";
      resp->data.assign(buf.begin(), buf.begin() + std::min(static_cast<size_t>(rc), buf.size()));
      return;
    }

    resp->success = false;
    resp->message = "read failed: " + describe_rc(rc);
  }

  void on_write(
    const std::shared_ptr<peripherals_nfc_node::srv::NfcWriteBlock::Request> req,
    std::shared_ptr<peripherals_nfc_node::srv::NfcWriteBlock::Response> resp)
  {
    if (req->data.size() != kBlockLen) {
      resp->success = false;
      resp->message = "write failed: data must contain exactly 16 bytes";
      return;
    }

    std::lock_guard<std::mutex> lock(device_mutex_);

    std::array<uint8_t, kBlockLen> buf {};
    std::copy(req->data.begin(), req->data.end(), buf.begin());

    const int rc = nfc_write_block(dev_, req->block_addr, buf.data(), buf.size());
    resp->success = (rc > 0);
    resp->message = resp->success ? "ok" : "write failed: " + describe_rc(rc);
  }

  std::string transport_;
  std::string driver_;
  std::string instance_name_;
  std::string device_;
  int i2c_addr_{0x28};
  int cs_pin_{0};
  int baud_{115200};
  std::string frame_id_;

  std::string tag_topic_;
  std::string poll_service_name_;
  std::string read_service_name_;
  std::string write_service_name_;

  bool auto_poll_enabled_{true};
  int poll_period_ms_{100};
  int poll_timeout_ms_{50};
  bool publish_duplicates_{false};

  std::mutex device_mutex_;
  nfc_dev * dev_{nullptr};
  struct nfc_tag_info last_tag_info_ {};
  bool has_last_tag_{false};

  rclcpp::Publisher<peripherals_nfc_node::msg::NfcTagInfo>::SharedPtr tag_pub_;
  rclcpp::Service<peripherals_nfc_node::srv::NfcPoll>::SharedPtr poll_srv_;
  rclcpp::Service<peripherals_nfc_node::srv::NfcReadBlock>::SharedPtr read_srv_;
  rclcpp::Service<peripherals_nfc_node::srv::NfcWriteBlock>::SharedPtr write_srv_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<NfcNode>());
  } catch (const std::exception & e) {
    std::fprintf(stderr, "nfc_node exception: %s\n", e.what());
  }
  rclcpp::shutdown();
  return 0;
}
