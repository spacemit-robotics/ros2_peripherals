#include <key.h>

#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <peripherals_key_node/msg/key_event.hpp>
#include <rclcpp/rclcpp.hpp>

namespace
{
constexpr uint8_t kEventPressed = 1;
constexpr uint8_t kEventReleased = 2;
constexpr uint8_t kEventClick = 3;
constexpr uint8_t kEventDoubleClick = 4;
constexpr uint8_t kEventLongPressStart = 5;
constexpr uint8_t kEventRepeat = 8;

uint8_t to_ros_event_type(key_event_t event)
{
    switch (event) {
        case KEY_EV_PRESSED:
            return kEventPressed;
        case KEY_EV_RELEASED:
            return kEventReleased;
        case KEY_EV_CLICK:
            return kEventClick;
        case KEY_EV_DOUBLE_CLICK:
            return kEventDoubleClick;
        case KEY_EV_LONG_PRESS:
            return kEventLongPressStart;
        case KEY_EV_HOLD_REPEAT:
            return kEventRepeat;
        default:
            return 0;
    }
}

std::vector<int64_t> load_int_array(
    rclcpp::Node * node, const std::string & name, const std::vector<int64_t> & default_value)
{
    node->declare_parameter(name, default_value);
    return node->get_parameter(name).as_integer_array();
}

std::vector<std::string> load_string_array(
    rclcpp::Node * node, const std::string & name,
    const std::vector<std::string> & default_value)
{
    node->declare_parameter(name, default_value);
    return node->get_parameter(name).as_string_array();
}
}  // namespace

class KeyNode : public rclcpp::Node
{
public:
    KeyNode()
    : Node("key_node")
    {
        try {
            const auto topic_name =
                declare_parameter<std::string>("event_topic", "key/events");
            frame_id_ = declare_parameter<std::string>("frame_id", "key");
            const auto publish_period_ms =
                declare_parameter<int64_t>("publish_period_ms", 10);

            const auto key_ids = load_int_array(this, "key_ids", {0});
            const auto gpio_nums = load_int_array(this, "gpio_nums", {74});
            const auto active_lows = load_int_array(this, "active_lows", {1});
            const auto long_press_mss = load_int_array(this, "long_press_mss", {1500});
            const auto double_click_mss = load_int_array(this, "double_click_mss", {300});
            const auto key_names = load_string_array(this, "key_names", {});

            validate_config(
                key_ids, gpio_nums, active_lows, long_press_mss, double_click_mss, key_names);

            publisher_ = create_publisher<peripherals_key_node::msg::KeyEvent>(topic_name, 32);

            if (key_service_start() != 0) {
                throw std::runtime_error("key_service_start() failed");
            }
            service_started_ = true;

            keys_.reserve(key_ids.size());
            for (size_t i = 0; i < key_ids.size(); ++i) {
                register_key(
                    static_cast<uint32_t>(key_ids[i]),
                    static_cast<int>(gpio_nums[i]),
                    active_lows[i] != 0,
                    static_cast<int>(long_press_mss[i]),
                    static_cast<int>(double_click_mss[i]),
                    key_names.empty() ? default_key_name(key_ids[i], gpio_nums[i]) : key_names[i]);
            }

            drain_timer_ = create_wall_timer(
                std::chrono::milliseconds(publish_period_ms),
                std::bind(&KeyNode::drain_events, this));

            RCLCPP_INFO(
                get_logger(), "key_node ready: topic=%s, keys=%zu",
                publisher_->get_topic_name(), keys_.size());
        } catch (...) {
            cleanup_resources();
            throw;
        }
    }

    ~KeyNode() override
    {
        cleanup_resources();
    }

private:
    struct EventRecord
    {
        uint32_t key_id;
        uint8_t event_type;
    };

    struct KeyContext
    {
        KeyNode * node;
        uint32_t key_id;
        std::string name;
    };

    struct RegisteredKey
    {
        std::unique_ptr<KeyContext> context;
        struct key_handle * handle{nullptr};
    };

    static void on_key_event(struct key_handle * /*key*/, key_event_t event, void * user_data)
    {
        auto * context = static_cast<KeyContext *>(user_data);
        if (context == nullptr || context->node == nullptr) {
            return;
        }

        const auto ros_event = to_ros_event_type(event);
        if (ros_event == 0) {
            return;
        }

        context->node->enqueue_event(context->key_id, ros_event);
    }

    void enqueue_event(uint32_t key_id, uint8_t event_type)
    {
        std::lock_guard<std::mutex> lock(event_mutex_);
        event_queue_.push_back(EventRecord{key_id, event_type});
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
        for (const auto & event : local_queue) {
            peripherals_key_node::msg::KeyEvent msg;
            msg.header.stamp = stamp;
            msg.header.frame_id = frame_id_;
            msg.key_id = event.key_id;
            msg.event_type = event.event_type;
            publisher_->publish(msg);
        }
    }

    void register_key(
        uint32_t key_id, int gpio_num, bool active_low, int long_press_ms, int double_click_ms,
        const std::string & name)
    {
        auto context = std::make_unique<KeyContext>();
        context->node = this;
        context->key_id = key_id;
        context->name = name;

        key_config_t config{};
        config.gpio_num = gpio_num;
        config.active_low = active_low ? 1 : 0;
        config.long_press_ms = long_press_ms;
        config.double_click_ms = double_click_ms;

        auto * handle = key_add_gpio(&config, &KeyNode::on_key_event, context.get());
        if (handle == nullptr) {
            throw std::runtime_error(
                "key_add_gpio() failed for key_id=" + std::to_string(key_id) +
                ", gpio_num=" + std::to_string(gpio_num));
        }

        RCLCPP_INFO(
            get_logger(),
            "registered key: key_id=%u gpio=%d active_low=%s long_press_ms=%d double_click_ms=%d name=%s",
            key_id, gpio_num, active_low ? "true" : "false", long_press_ms, double_click_ms,
            name.c_str());

        RegisteredKey registered_key;
        registered_key.context = std::move(context);
        registered_key.handle = handle;
        keys_.push_back(std::move(registered_key));
    }

    void validate_config(
        const std::vector<int64_t> & key_ids,
        const std::vector<int64_t> & gpio_nums,
        const std::vector<int64_t> & active_lows,
        const std::vector<int64_t> & long_press_mss,
        const std::vector<int64_t> & double_click_mss,
        const std::vector<std::string> & key_names)
    {
        const auto key_count = key_ids.size();
        if (key_count == 0) {
            throw std::invalid_argument("key_ids must not be empty");
        }

        const auto same_size = [key_count](size_t size, const char * field_name) {
            if (size != key_count) {
                throw std::invalid_argument(
                    std::string(field_name) + " size mismatch with key_ids");
            }
        };

        same_size(gpio_nums.size(), "gpio_nums");
        same_size(active_lows.size(), "active_lows");
        same_size(long_press_mss.size(), "long_press_mss");
        same_size(double_click_mss.size(), "double_click_mss");
        if (!key_names.empty()) {
            same_size(key_names.size(), "key_names");
        }

        for (size_t i = 0; i < key_count; ++i) {
            if (key_ids[i] < 0) {
                throw std::invalid_argument("key_ids must be >= 0");
            }
            if (gpio_nums[i] <= 0) {
                throw std::invalid_argument("gpio_nums must be > 0");
            }
            if (long_press_mss[i] < 0 || double_click_mss[i] < 0) {
                throw std::invalid_argument("long_press_mss/double_click_mss must be >= 0");
            }
        }
    }

    std::string default_key_name(int64_t key_id, int64_t gpio_num) const
    {
        return "key_" + std::to_string(key_id) + "_gpio_" + std::to_string(gpio_num);
    }

    void cleanup_resources()
    {
        drain_timer_.reset();

        for (auto & key : keys_) {
            if (key.handle != nullptr) {
                key_remove(key.handle);
                key.handle = nullptr;
            }
        }

        if (service_started_) {
            key_service_stop();
            service_started_ = false;
        }
    }

    std::string frame_id_;
    bool service_started_{false};
    rclcpp::Publisher<peripherals_key_node::msg::KeyEvent>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr drain_timer_;
    std::mutex event_mutex_;
    std::deque<EventRecord> event_queue_;
    std::vector<RegisteredKey> keys_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<KeyNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
