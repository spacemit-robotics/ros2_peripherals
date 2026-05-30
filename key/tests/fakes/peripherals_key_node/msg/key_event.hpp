#ifndef FAKE_KEY_EVENT_HPP  // NOLINT(build/header_guard)
#define FAKE_KEY_EVENT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>

#include <std_msgs/msg/header.hpp>

namespace peripherals_key_node
{
namespace msg
{
struct KeyEvent
{
    using SharedPtr = std::shared_ptr<KeyEvent>;

    std_msgs::msg::Header header;
    uint32_t key_id{0};
    uint8_t event_type{0};
};
}  // namespace msg
}  // namespace peripherals_key_node
#endif  // FAKE_KEY_EVENT_HPP
