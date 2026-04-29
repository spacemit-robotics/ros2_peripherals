#include <misc_io.h>

#include <rclcpp/rclcpp.hpp>

#include <peripherals_misc_io_node/msg/misc_io_command.hpp>
#include <peripherals_misc_io_node/msg/misc_io_event.hpp>
#include <peripherals_misc_io_node/msg/misc_io_state.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
std::vector<int64_t> load_int_array(
  rclcpp::Node & node, const std::string & name, const std::vector<int64_t> & default_value)
{
  node.declare_parameter(name, default_value);
  return node.get_parameter(name).as_integer_array();
}

std::vector<std::string> load_string_array(
  rclcpp::Node & node, const std::string & name, const std::vector<std::string> & default_value)
{
  node.declare_parameter(name, default_value);
  return node.get_parameter(name).as_string_array();
}

bool is_valid_misc_type(int64_t value)
{
  return value >= static_cast<int64_t>(MISC_TYPE_GENERIC) &&
         value <= static_cast<int64_t>(MISC_TYPE_SENSOR);
}

bool is_valid_misc_dir(int64_t value)
{
  return value == static_cast<int64_t>(MISC_DIR_INPUT) ||
         value == static_cast<int64_t>(MISC_DIR_OUTPUT);
}

bool is_valid_misc_logic(int64_t value)
{
  return value == static_cast<int64_t>(MISC_ACTIVE_LOW) ||
         value == static_cast<int64_t>(MISC_ACTIVE_HIGH);
}

std::string default_consumer_name(int64_t io_id)
{
  return "misc_io_" + std::to_string(io_id);
}

std::string default_io_name(int64_t io_id, const std::string & chip_name, int64_t line_offset)
{
  return "io_" + std::to_string(io_id) + "_" + chip_name + "_" + std::to_string(line_offset);
}
}  // namespace

class MiscIoNode final : public rclcpp::Node
{
public:
  MiscIoNode()
  : rclcpp::Node("misc_io_node")
  {
    try {
      command_topic_ = declare_parameter<std::string>("command_topic", "misc_io/command");
      state_topic_ = declare_parameter<std::string>("state_topic", "misc_io/state");
      event_topic_ = declare_parameter<std::string>("event_topic", "misc_io/events");
      frame_id_ = declare_parameter<std::string>("frame_id", "misc_io");
      publish_period_ms_ = declare_parameter<int64_t>("publish_period_ms", 100);

      const auto io_ids = load_int_array(*this, "io_ids", {0});
      const auto types = load_int_array(
        *this, "types", {static_cast<int64_t>(MISC_TYPE_GENERIC)});
      const auto dirs = load_int_array(
        *this, "dirs", {static_cast<int64_t>(MISC_DIR_OUTPUT)});
      const auto active_logics = load_int_array(
        *this, "active_logics", {static_cast<int64_t>(MISC_ACTIVE_HIGH)});
      const auto debounce_mss = load_int_array(*this, "debounce_mss", {10});
      const auto chip_names = load_string_array(*this, "chip_names", {"gpiochip0"});
      const auto line_offsets = load_int_array(*this, "line_offsets", {0});
      const auto consumers = load_string_array(*this, "consumers", {});
      const auto io_names = load_string_array(*this, "io_names", {});

      validate_config(
        io_ids, types, dirs, active_logics, debounce_mss, chip_names, line_offsets, consumers,
        io_names);

      state_pub_ = create_publisher<peripherals_misc_io_node::msg::MiscIoState>(
        state_topic_,
        rclcpp::QoS(rclcpp::KeepLast(std::max<size_t>(1, io_ids.size())))
        .reliable()
        .transient_local());
      event_pub_ = create_publisher<peripherals_misc_io_node::msg::MiscIoEvent>(
        event_topic_, rclcpp::QoS(32).reliable());
      cmd_sub_ = create_subscription<peripherals_misc_io_node::msg::MiscIoCommand>(
        command_topic_, rclcpp::QoS(10).reliable(),
        std::bind(&MiscIoNode::on_command, this, std::placeholders::_1));

      ios_.reserve(io_ids.size());
      io_index_.reserve(io_ids.size());
      for (size_t i = 0; i < io_ids.size(); ++i) {
        register_io(
          static_cast<uint32_t>(io_ids[i]),
          static_cast<enum misc_type>(types[i]),
          static_cast<enum misc_dir>(dirs[i]),
          static_cast<enum misc_logic>(active_logics[i]),
          static_cast<uint16_t>(debounce_mss[i]),
          chip_names[i],
          static_cast<unsigned int>(line_offsets[i]),
          consumers.empty() ? default_consumer_name(io_ids[i]) : consumers[i],
          io_names.empty() ? default_io_name(io_ids[i], chip_names[i], line_offsets[i]) : io_names[i]);
      }

      publish_all_states();

      state_timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&MiscIoNode::on_timer, this));

      RCLCPP_INFO(
        get_logger(),
        "misc_io_node ready: command_topic=%s state_topic=%s event_topic=%s ios=%zu",
        command_topic_.c_str(), state_topic_.c_str(), event_topic_.c_str(), ios_.size());
    } catch (...) {
      cleanup_resources();
      throw;
    }
  }

  ~MiscIoNode() override
  {
    cleanup_resources();
  }

private:
  struct EventRecord
  {
    uint32_t io_id;
    uint8_t event;
  };

  struct IoContext
  {
    MiscIoNode * node;
    uint32_t io_id;
  };

  struct RegisteredIo
  {
    uint32_t io_id;
    enum misc_type type;
    enum misc_dir dir;
    enum misc_logic active_logic;
    uint16_t debounce_ms;
    std::string chip_name;
    unsigned int line_offset;
    std::string consumer;
    std::string name;
    std::unique_ptr<IoContext> context;
    struct misc_dev * dev{nullptr};
  };

  static void on_input_event(
    struct misc_dev * /*dev*/, enum misc_event ev, void * args)
  {
    auto * context = static_cast<IoContext *>(args);
    if (context == nullptr || context->node == nullptr) {
      return;
    }
    context->node->enqueue_event(context->io_id, static_cast<uint8_t>(ev));
  }

  void enqueue_event(uint32_t io_id, uint8_t event)
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    event_queue_.push_back(EventRecord{io_id, event});
  }

  void validate_config(
    const std::vector<int64_t> & io_ids,
    const std::vector<int64_t> & types,
    const std::vector<int64_t> & dirs,
    const std::vector<int64_t> & active_logics,
    const std::vector<int64_t> & debounce_mss,
    const std::vector<std::string> & chip_names,
    const std::vector<int64_t> & line_offsets,
    const std::vector<std::string> & consumers,
    const std::vector<std::string> & io_names) const
  {
    const auto io_count = io_ids.size();
    if (io_count == 0) {
      throw std::invalid_argument("io_ids must not be empty");
    }
    if (command_topic_.empty()) {
      throw std::invalid_argument("command_topic must not be empty");
    }
    if (state_topic_.empty()) {
      throw std::invalid_argument("state_topic must not be empty");
    }
    if (event_topic_.empty()) {
      throw std::invalid_argument("event_topic must not be empty");
    }
    if (frame_id_.empty()) {
      throw std::invalid_argument("frame_id must not be empty");
    }
    if (publish_period_ms_ <= 0) {
      throw std::invalid_argument("publish_period_ms must be > 0");
    }

    const auto same_size = [io_count](size_t size, const char * field_name) {
        if (size != io_count) {
          throw std::invalid_argument(
                  std::string(field_name) + " size mismatch with io_ids");
        }
      };

    same_size(types.size(), "types");
    same_size(dirs.size(), "dirs");
    same_size(active_logics.size(), "active_logics");
    same_size(debounce_mss.size(), "debounce_mss");
    same_size(chip_names.size(), "chip_names");
    same_size(line_offsets.size(), "line_offsets");
    if (!consumers.empty()) {
      same_size(consumers.size(), "consumers");
    }
    if (!io_names.empty()) {
      same_size(io_names.size(), "io_names");
    }

    std::unordered_set<uint32_t> seen_ids;
    for (size_t i = 0; i < io_count; ++i) {
      if (io_ids[i] < 0) {
        throw std::invalid_argument("io_ids must be >= 0");
      }
      if (!seen_ids.insert(static_cast<uint32_t>(io_ids[i])).second) {
        throw std::invalid_argument("io_ids must be unique");
      }
      if (!is_valid_misc_type(types[i])) {
        throw std::invalid_argument("types contains invalid value");
      }
      if (!is_valid_misc_dir(dirs[i])) {
        throw std::invalid_argument("dirs contains invalid value");
      }
      if (!is_valid_misc_logic(active_logics[i])) {
        throw std::invalid_argument("active_logics contains invalid value");
      }
      if (debounce_mss[i] < 0 || debounce_mss[i] > 65535) {
        throw std::invalid_argument("debounce_mss must be in [0, 65535]");
      }
      if (chip_names[i].empty()) {
        throw std::invalid_argument("chip_names must not be empty");
      }
      if (line_offsets[i] < 0) {
        throw std::invalid_argument("line_offsets must be >= 0");
      }
    }
  }

  void register_io(
    uint32_t io_id, enum misc_type type, enum misc_dir dir, enum misc_logic active_logic,
    uint16_t debounce_ms, const std::string & chip_name, unsigned int line_offset,
    const std::string & consumer, const std::string & name)
  {
    RegisteredIo io;
    io.io_id = io_id;
    io.type = type;
    io.dir = dir;
    io.active_logic = active_logic;
    io.debounce_ms = debounce_ms;
    io.chip_name = chip_name;
    io.line_offset = line_offset;
    io.consumer = consumer;
    io.name = name;
    io.context = std::make_unique<IoContext>();
    io.context->node = this;
    io.context->io_id = io_id;

    struct misc_gpiod_ctx ctx {};
    ctx.chip_name = io.chip_name.c_str();
    ctx.line_offset = io.line_offset;
    ctx.consumer = io.consumer.empty() ? nullptr : io.consumer.c_str();

    io.dev = misc_io_alloc(io.type, io.dir, &ctx);
    if (io.dev == nullptr) {
      throw std::runtime_error(
              "misc_io_alloc failed for io_id=" + std::to_string(io_id) +
              ", chip=" + chip_name + ", line_offset=" + std::to_string(line_offset));
    }

    misc_io_config(io.dev, io.active_logic, io.debounce_ms);
    if (io.dir == MISC_DIR_INPUT) {
      misc_io_trigger(io.dev, &MiscIoNode::on_input_event, io.context.get());
    }

    io_index_[io_id] = ios_.size();
    RCLCPP_INFO(
      get_logger(),
      "registered io: io_id=%u type=%u dir=%u active_logic=%u debounce_ms=%u chip=%s line_offset=%u consumer=%s name=%s",
      io.io_id, static_cast<unsigned int>(io.type), static_cast<unsigned int>(io.dir),
      static_cast<unsigned int>(io.active_logic), static_cast<unsigned int>(io.debounce_ms),
      io.chip_name.c_str(), io.line_offset, io.consumer.c_str(), io.name.c_str());
    ios_.push_back(std::move(io));
  }

  void on_command(const peripherals_misc_io_node::msg::MiscIoCommand::SharedPtr msg)
  {
    if (msg == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock(api_mutex_);
    auto it = io_index_.find(msg->io_id);
    if (it == io_index_.end()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 3000, "received command for unknown io_id=%u", msg->io_id);
      return;
    }

    auto & io = ios_[it->second];
    if (io.dir != MISC_DIR_OUTPUT) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 3000, "received command for input io_id=%u", msg->io_id);
      return;
    }

    const int rc = misc_io_set(io.dev, msg->active);
    if (rc != 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 3000, "misc_io_set failed for io_id=%u rc=%d", msg->io_id, rc);
      return;
    }

    publish_state_locked(io, now());
  }

  void on_timer()
  {
    drain_events();
    publish_all_states();
  }

  void drain_events()
  {
    std::deque<EventRecord> local_queue;
    {
      std::lock_guard<std::mutex> lock(event_mutex_);
      if (event_queue_.empty()) {
        return;
      }
      local_queue.swap(event_queue_);
    }

    const auto stamp = now();
    for (const auto & record : local_queue) {
      peripherals_misc_io_node::msg::MiscIoEvent msg;
      msg.header.stamp = stamp;
      msg.header.frame_id = frame_id_;
      msg.io_id = record.io_id;
      msg.event = record.event;
      event_pub_->publish(msg);
    }
  }

  void publish_all_states()
  {
    std::lock_guard<std::mutex> lock(api_mutex_);
    const auto stamp = now();
    for (const auto & io : ios_) {
      publish_state_locked(io, stamp);
    }
  }

  void publish_state_locked(const RegisteredIo & io, const rclcpp::Time & stamp)
  {
    const int value = misc_io_get(io.dev);
    if (value < 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 3000, "misc_io_get failed for io_id=%u rc=%d", io.io_id,
        value);
      return;
    }

    peripherals_misc_io_node::msg::MiscIoState msg;
    msg.header.stamp = stamp;
    msg.header.frame_id = frame_id_;
    msg.io_id = io.io_id;
    msg.type = static_cast<uint8_t>(io.type);
    msg.dir = static_cast<uint8_t>(io.dir);
    msg.active = (value != 0);
    state_pub_->publish(msg);
  }

  void cleanup_resources()
  {
    std::lock_guard<std::mutex> lock(api_mutex_);
    io_index_.clear();
    for (auto & io : ios_) {
      if (io.dev != nullptr) {
        misc_io_free(io.dev);
        io.dev = nullptr;
      }
    }
    ios_.clear();
  }

  std::string command_topic_;
  std::string state_topic_;
  std::string event_topic_;
  std::string frame_id_;
  int64_t publish_period_ms_{100};

  std::mutex api_mutex_;
  std::mutex event_mutex_;
  std::deque<EventRecord> event_queue_;
  std::vector<RegisteredIo> ios_;
  std::unordered_map<uint32_t, size_t> io_index_;

  rclcpp::Subscription<peripherals_misc_io_node::msg::MiscIoCommand>::SharedPtr cmd_sub_;
  rclcpp::Publisher<peripherals_misc_io_node::msg::MiscIoState>::SharedPtr state_pub_;
  rclcpp::Publisher<peripherals_misc_io_node::msg::MiscIoEvent>::SharedPtr event_pub_;
  rclcpp::TimerBase::SharedPtr state_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<MiscIoNode>());
  } catch (const std::exception & e) {
    std::fprintf(stderr, "misc_io_node exception: %s\n", e.what());
  }
  rclcpp::shutdown();
  return 0;
}
