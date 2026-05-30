#ifndef FAKE_LED_STATE_HPP  // NOLINT(build/header_guard)
#define FAKE_LED_STATE_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>

#include <std_msgs/msg/header.hpp>

namespace peripherals_led_node
{
namespace msg
{
struct LedState
{
    using SharedPtr = std::shared_ptr<LedState>;

    std_msgs::msg::Header header;
    uint32_t led_id{0};
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t brightness{0};
    uint8_t mode{0};
    bool is_on{false};
};
}  // namespace msg
}  // namespace peripherals_led_node
#endif  // FAKE_LED_STATE_HPP
