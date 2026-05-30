#ifndef FAKE_MISC_IO_STATE_HPP  // NOLINT(build/header_guard)
#define FAKE_MISC_IO_STATE_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>

#include <std_msgs/msg/header.hpp>

namespace peripherals_misc_io_node
{
namespace msg
{
struct MiscIoState
{
    using SharedPtr = std::shared_ptr<MiscIoState>;

    std_msgs::msg::Header header;
    uint32_t io_id{0};
    uint8_t type{0};
    uint8_t dir{0};
    bool active{false};
};
}  // namespace msg
}  // namespace peripherals_misc_io_node
#endif  // FAKE_MISC_IO_STATE_HPP
