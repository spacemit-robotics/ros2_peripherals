#ifndef FAKE_LED_COMMAND_HPP  // NOLINT(build/header_guard)
#define FAKE_LED_COMMAND_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>

#include <std_msgs/msg/header.hpp>

namespace peripherals_led_node
{
namespace msg
{
struct LedCommand
{
    using SharedPtr = std::shared_ptr<LedCommand>;

    std_msgs::msg::Header header;
    uint32_t led_id{0};
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t brightness{0};
    uint8_t mode{0};
    uint16_t period_ms{0};
    uint16_t on_ms{0};
    uint8_t count{0};
};
}  // namespace msg
}  // namespace peripherals_led_node
#endif  // FAKE_LED_COMMAND_HPP
