extern "C"
{
#include <led.h>
}

#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <peripherals_led_node/msg/led_command.hpp>
#include <peripherals_led_node/msg/led_state.hpp>

namespace
{
constexpr uint8_t kLedModeStatic = 0;
constexpr uint8_t kLedModeBlink = 1;
constexpr uint8_t kLedModeBreath = 2;

std::string resolved_alloc_name(const std::string & transport, const std::string & name)
{
    if (transport == "spi" && name.find(':') == std::string::npos) {
        return "spi-ws2812:" + name;
    }
    return name;
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

class LedNode final : public rclcpp::Node
{
public:
    LedNode()
    : rclcpp::Node("led_node")
    {
        try {
            frame_id_ = declare_parameter<std::string>("frame_id", "led");
            command_topic_ = declare_parameter<std::string>("command_topic", "led/command");
            state_topic_ = declare_parameter<std::string>("state_topic", "led/state");
            tick_period_ms_ = declare_parameter<int64_t>("tick_period_ms", 50);
            publish_period_ms_ = declare_parameter<int64_t>("publish_period_ms", 100);
            publish_on_command_ = declare_parameter<bool>("publish_on_command", true);
            publish_on_startup_ = declare_parameter<bool>("publish_on_startup", true);

            const auto led_ids = load_int_array(this, "led_ids", {0});
            const auto transports = load_string_array(this, "transports", {"generic"});
            const auto names = load_string_array(this, "names", {"sys-led_1"});

            const auto generic_sysfs_names =
                load_string_array(this, "generic_sysfs_names", {""});
            const auto generic_active_levels =
                load_int_array(this, "generic_active_levels", {0});

            const auto spi_dev_paths =
                load_string_array(this, "spi_dev_paths", {"/dev/spidev2.0"});
            const auto spi_num_leds = load_int_array(this, "spi_num_leds", {1});
            const auto spi_speed_hz = load_int_array(this, "spi_speed_hz", {6400000});
            const auto spi_reset_bytes = load_int_array(this, "spi_reset_bytes", {80});

            validate_config(
                led_ids, transports, names, generic_sysfs_names, generic_active_levels,
                spi_dev_paths, spi_num_leds, spi_speed_hz, spi_reset_bytes);

            state_pub_ = create_publisher<peripherals_led_node::msg::LedState>(
                state_topic_,
                rclcpp::QoS(rclcpp::KeepLast(std::max<size_t>(1, led_ids.size())))
                    .reliable()
                    .transient_local());
            cmd_sub_ = create_subscription<peripherals_led_node::msg::LedCommand>(
                command_topic_, rclcpp::QoS(10).reliable(),
                std::bind(&LedNode::on_command, this, std::placeholders::_1));

            leds_.reserve(led_ids.size());
            led_index_.reserve(led_ids.size());
            for (size_t i = 0; i < led_ids.size(); ++i) {
                register_led(
                    static_cast<uint32_t>(led_ids[i]), transports[i], names[i],
                    generic_sysfs_names[i], static_cast<int>(generic_active_levels[i]),
                    spi_dev_paths[i], static_cast<uint32_t>(spi_num_leds[i]),
                    static_cast<uint32_t>(spi_speed_hz[i]),
                    static_cast<uint32_t>(spi_reset_bytes[i]));
            }

            if (publish_on_startup_) {
                publish_all_states();
            }

            tick_timer_ = create_wall_timer(
                std::chrono::milliseconds(tick_period_ms_),
                std::bind(&LedNode::on_tick_timer, this));
            publish_timer_ = create_wall_timer(
                std::chrono::milliseconds(publish_period_ms_),
                std::bind(&LedNode::publish_all_states, this));

            RCLCPP_INFO_STREAM(
                get_logger(),
                "led_node ready: command_topic=" << command_topic_
                << " state_topic=" << state_topic_
                << " leds=" << leds_.size()
                << " tick_period_ms=" << tick_period_ms_
                << " publish_period_ms=" << publish_period_ms_);
        } catch (...) {
            cleanup_resources();
            throw;
        }
    }

    ~LedNode() override
    {
        cleanup_resources();
    }

private:
    struct RegisteredLed
    {
        uint32_t led_id{0};
        std::string transport;
        std::string name;

        std::string generic_sysfs_name;
        int generic_active_level{0};

        std::string spi_dev_path;
        uint32_t spi_num_leds{1};
        uint32_t spi_speed_hz{6400000};
        uint32_t spi_reset_bytes{80};

        struct led_dev * dev{nullptr};

        uint8_t red{0};
        uint8_t green{0};
        uint8_t blue{0};
        uint8_t brightness{0};
        uint8_t mode{kLedModeStatic};
        uint16_t period_ms{0};
        uint16_t on_ms{0};
        uint8_t count{0};
        bool is_on{false};

        uint16_t blink_elapsed_ms{0};
        uint8_t blink_done{0};
        bool blink_on{false};
        uint16_t breath_elapsed_ms{0};
    };

    struct LedSysfsArgs
    {
        const char * sysfs_name;
        int active_level;
    };

    struct LedWs2812Args
    {
        const char * dev_path;
        uint32_t num_leds;
        uint32_t spi_speed_hz;
        uint32_t reset_bytes;
    };

    void validate_config(
        const std::vector<int64_t> & led_ids,
        const std::vector<std::string> & transports,
        const std::vector<std::string> & names,
        const std::vector<std::string> & generic_sysfs_names,
        const std::vector<int64_t> & generic_active_levels,
        const std::vector<std::string> & spi_dev_paths,
        const std::vector<int64_t> & spi_num_leds,
        const std::vector<int64_t> & spi_speed_hz,
        const std::vector<int64_t> & spi_reset_bytes) const
    {
        const auto led_count = led_ids.size();
        if (led_count == 0) {
            throw std::invalid_argument("led_ids must not be empty");
        }
        if (frame_id_.empty()) {
            throw std::invalid_argument("frame_id must not be empty");
        }
        if (command_topic_.empty()) {
            throw std::invalid_argument("command_topic must not be empty");
        }
        if (state_topic_.empty()) {
            throw std::invalid_argument("state_topic must not be empty");
        }
        if (tick_period_ms_ <= 0) {
            throw std::invalid_argument("tick_period_ms must be > 0");
        }
        if (publish_period_ms_ <= 0) {
            throw std::invalid_argument("publish_period_ms must be > 0");
        }

        const auto same_size = [led_count](size_t size, const char * field_name) {
            if (size != led_count) {
                throw std::invalid_argument(
                    std::string(field_name) + " size mismatch with led_ids");
            }
        };

        same_size(transports.size(), "transports");
        same_size(names.size(), "names");
        same_size(generic_sysfs_names.size(), "generic_sysfs_names");
        same_size(generic_active_levels.size(), "generic_active_levels");
        same_size(spi_dev_paths.size(), "spi_dev_paths");
        same_size(spi_num_leds.size(), "spi_num_leds");
        same_size(spi_speed_hz.size(), "spi_speed_hz");
        same_size(spi_reset_bytes.size(), "spi_reset_bytes");

        std::unordered_set<uint32_t> seen_ids;
        for (size_t i = 0; i < led_count; ++i) {
            if (led_ids[i] < 0) {
                throw std::invalid_argument("led_ids must be >= 0");
            }
            if (!seen_ids.insert(static_cast<uint32_t>(led_ids[i])).second) {
                throw std::invalid_argument("led_ids must be unique");
            }
            if (transports[i] != "generic" && transports[i] != "spi") {
                throw std::invalid_argument("transports must be one of: generic, spi");
            }
            if (names[i].empty()) {
                throw std::invalid_argument("names entries must not be empty");
            }
            if (generic_active_levels[i] != 0 && generic_active_levels[i] != 1) {
                throw std::invalid_argument("generic_active_levels entries must be 0 or 1");
            }
            if (spi_num_leds[i] <= 0) {
                throw std::invalid_argument("spi_num_leds entries must be > 0");
            }
            if (spi_speed_hz[i] <= 0) {
                throw std::invalid_argument("spi_speed_hz entries must be > 0");
            }
            if (spi_reset_bytes[i] < 0) {
                throw std::invalid_argument("spi_reset_bytes entries must be >= 0");
            }
        }
    }

    void register_led(
        uint32_t led_id,
        const std::string & transport,
        const std::string & name,
        const std::string & generic_sysfs_name,
        int generic_active_level,
        const std::string & spi_dev_path,
        uint32_t spi_num_leds,
        uint32_t spi_speed_hz,
        uint32_t spi_reset_bytes)
    {
        RegisteredLed led;
        led.led_id = led_id;
        led.transport = transport;
        led.name = name;
        led.generic_sysfs_name = generic_sysfs_name;
        led.generic_active_level = generic_active_level;
        led.spi_dev_path = spi_dev_path;
        led.spi_num_leds = spi_num_leds;
        led.spi_speed_hz = spi_speed_hz;
        led.spi_reset_bytes = spi_reset_bytes;

        const std::string alloc_name = resolved_alloc_name(transport, name);
        if (transport == "generic") {
            LedSysfsArgs args{};
            args.sysfs_name = generic_sysfs_name.empty() ? nullptr : generic_sysfs_name.c_str();
            args.active_level = generic_active_level;
            led.dev = led_alloc_generic(alloc_name.c_str(), &args);
        } else if (transport == "spi") {
            LedWs2812Args args{};
            args.dev_path = spi_dev_path.c_str();
            args.num_leds = spi_num_leds;
            args.spi_speed_hz = spi_speed_hz;
            args.reset_bytes = spi_reset_bytes;
            led.dev = led_alloc_spi(alloc_name.c_str(), &args);
        }

        if (led.dev == nullptr) {
            throw std::runtime_error(
                "failed to allocate LED device id=" + std::to_string(led_id) +
                " transport=" + transport + " name=" + name);
        }

        led_index_.emplace(led_id, leds_.size());
        leds_.push_back(std::move(led));
    }

    void cleanup_resources()
    {
        std::lock_guard<std::mutex> lock(api_mutex_);
        for (auto & led : leds_) {
            if (led.dev != nullptr) {
                led_free(led.dev);
                led.dev = nullptr;
            }
        }
        leds_.clear();
        led_index_.clear();
    }

    void on_tick_timer()
    {
        std::lock_guard<std::mutex> lock(api_mutex_);
        for (auto & led : leds_) {
            if (led.dev != nullptr) {
                led_tick(led.dev, static_cast<uint16_t>(tick_period_ms_));
                mirror_tick_state(&led, static_cast<uint16_t>(tick_period_ms_));
            }
        }
    }

    void on_command(const peripherals_led_node::msg::LedCommand::SharedPtr msg)
    {
        bool should_publish = false;
        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            const auto it = led_index_.find(msg->led_id);
            if (it == led_index_.end()) {
                RCLCPP_WARN(get_logger(), "unknown led_id=%u", msg->led_id);
                return;
            }

            RegisteredLed * led = &leds_[it->second];
            apply_command_locked(led, *msg);
            should_publish = publish_on_command_;
        }

        if (should_publish) {
            publish_led_state(msg->led_id);
        }
    }

    void apply_command_locked(
        RegisteredLed * led, const peripherals_led_node::msg::LedCommand & cmd)
    {
        if (cmd.mode > kLedModeBreath) {
            RCLCPP_WARN(
                get_logger(), "invalid LED mode=%u for led_id=%u", cmd.mode, cmd.led_id);
            return;
        }
        if ((cmd.mode == kLedModeBlink || cmd.mode == kLedModeBreath) && cmd.period_ms == 0) {
            RCLCPP_WARN(
                get_logger(), "period_ms must be > 0 for led_id=%u mode=%u", cmd.led_id,
                cmd.mode);
            return;
        }

        led->red = cmd.r;
        led->green = cmd.g;
        led->blue = cmd.b;
        led->mode = cmd.mode;
        led->period_ms = cmd.period_ms;
        led->on_ms = cmd.on_ms;
        led->count = cmd.count;
        led->blink_elapsed_ms = 0;
        led->blink_done = 0;
        led->blink_on = false;
        led->breath_elapsed_ms = 0;

        struct led_color color{};
        color.r = cmd.r;
        color.g = cmd.g;
        color.b = cmd.b;
        led_set_color(led->dev, &color);

        switch (cmd.mode) {
            case kLedModeStatic:
            {
                led->brightness = cmd.brightness;
                if (cmd.brightness == 0 || (cmd.r == 0 && cmd.g == 0 && cmd.b == 0)) {
                    led_set_state(led->dev, false);
                    led->is_on = false;
                    led->brightness = 0;
                } else {
                    led_set_brightness(led->dev, cmd.brightness);
                    led->is_on = true;
                }
                break;
            }
            case kLedModeBlink:
            {
                struct led_blink_param blink{};
                blink.period_ms = cmd.period_ms;
                blink.on_ms = std::min<uint16_t>(cmd.on_ms, cmd.period_ms);
                blink.count = cmd.count;
                led_blink(led->dev, &blink);
                led->period_ms = blink.period_ms;
                led->on_ms = blink.on_ms;
                led->blink_on = (blink.on_ms > 0);
                led->is_on = led->blink_on;
                led->brightness = led->blink_on ? 255 : 0;
                break;
            }
            case kLedModeBreath:
            {
                led_breath(led->dev, cmd.period_ms);
                led->brightness = 0;
                led->is_on = false;
                break;
            }
        }
    }

    void mirror_tick_state(RegisteredLed * led, uint16_t dt_ms)
    {
        if (led->mode == kLedModeBlink) {
            if (led->period_ms == 0) {
                led->mode = kLedModeStatic;
                led->is_on = false;
                led->brightness = 0;
                return;
            }

            const uint32_t total =
                static_cast<uint32_t>(led->blink_elapsed_ms) + static_cast<uint32_t>(dt_ms);
            const uint32_t cycles = total / led->period_ms;
            led->blink_elapsed_ms = static_cast<uint16_t>(total % led->period_ms);

            if (led->count != 0 && cycles > 0) {
                if (static_cast<uint32_t>(led->blink_done) + cycles >= led->count) {
                    led->blink_done = led->count;
                    led->mode = kLedModeStatic;
                    led->brightness = 0;
                    led->is_on = false;
                    led->blink_on = false;
                    return;
                }
                led->blink_done = static_cast<uint8_t>(led->blink_done + cycles);
            }

            led->blink_on = led->blink_elapsed_ms < led->on_ms;
            led->is_on = led->blink_on;
            led->brightness = led->blink_on ? 255 : 0;
            return;
        }

        if (led->mode == kLedModeBreath) {
            if (led->period_ms == 0) {
                led->mode = kLedModeStatic;
                led->is_on = false;
                led->brightness = 0;
                return;
            }

            uint32_t t =
                static_cast<uint32_t>(led->breath_elapsed_ms) + static_cast<uint32_t>(dt_ms);
            if (t >= led->period_ms) {
                t %= led->period_ms;
            }
            led->breath_elapsed_ms = static_cast<uint16_t>(t);

            const uint16_t half = static_cast<uint16_t>(led->period_ms / 2);
            if (half == 0) {
                led->brightness = 255;
            } else if (t < half) {
                led->brightness = static_cast<uint8_t>((255U * t) / half);
            } else {
                led->brightness =
                    static_cast<uint8_t>((255U * (led->period_ms - t)) / half);
            }
            led->is_on = led->brightness > 0;
            return;
        }

        led->is_on =
            led->brightness > 0 && (led->red != 0 || led->green != 0 || led->blue != 0);
    }

    peripherals_led_node::msg::LedState build_state_message(const RegisteredLed & led) const
    {
        peripherals_led_node::msg::LedState msg;
        msg.header.stamp = now();
        msg.header.frame_id = frame_id_;
        msg.led_id = led.led_id;
        msg.r = led.red;
        msg.g = led.green;
        msg.b = led.blue;
        msg.brightness = led.brightness;
        msg.mode = led.mode;
        msg.is_on = led.is_on;
        return msg;
    }

    void publish_led_state(uint32_t led_id)
    {
        peripherals_led_node::msg::LedState msg;
        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            const auto it = led_index_.find(led_id);
            if (it == led_index_.end()) {
                return;
            }
            msg = build_state_message(leds_[it->second]);
        }
        state_pub_->publish(msg);
    }

    void publish_all_states()
    {
        std::vector<peripherals_led_node::msg::LedState> messages;
        {
            std::lock_guard<std::mutex> lock(api_mutex_);
            messages.reserve(leds_.size());
            for (const auto & led : leds_) {
                messages.push_back(build_state_message(led));
            }
        }

        for (const auto & msg : messages) {
            state_pub_->publish(msg);
        }
    }

    std::string frame_id_;
    std::string command_topic_;
    std::string state_topic_;
    int64_t tick_period_ms_{50};
    int64_t publish_period_ms_{100};
    bool publish_on_command_{true};
    bool publish_on_startup_{true};

    std::mutex api_mutex_;
    std::vector<RegisteredLed> leds_;
    std::unordered_map<uint32_t, size_t> led_index_;

    rclcpp::Publisher<peripherals_led_node::msg::LedState>::SharedPtr state_pub_;
    rclcpp::Subscription<peripherals_led_node::msg::LedCommand>::SharedPtr cmd_sub_;
    rclcpp::TimerBase::SharedPtr tick_timer_;
    rclcpp::TimerBase::SharedPtr publish_timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<LedNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
