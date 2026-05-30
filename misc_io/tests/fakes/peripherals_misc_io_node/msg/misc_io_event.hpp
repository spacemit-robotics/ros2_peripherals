#ifndef FAKE_MISC_IO_EVENT_HPP  // NOLINT(build/header_guard)
#define FAKE_MISC_IO_EVENT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>

#include <std_msgs/msg/header.hpp>

namespace peripherals_misc_io_node
{
namespace msg
{
struct MiscIoEvent
{
    using SharedPtr = std::shared_ptr<MiscIoEvent>;

    std_msgs::msg::Header header;
    uint32_t io_id{0};
    uint8_t event{0};
};
}  // namespace msg
}  // namespace peripherals_misc_io_node
#endif  // FAKE_MISC_IO_EVENT_HPP
