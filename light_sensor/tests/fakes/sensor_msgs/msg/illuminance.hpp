#ifndef FAKE_ILLUMINANCE_HPP  // NOLINT(build/header_guard)
#define FAKE_ILLUMINANCE_HPP  // NOLINT(build/header_guard)

#include <memory>

#include <std_msgs/msg/header.hpp>

namespace sensor_msgs
{
namespace msg
{
struct Illuminance
{
    using SharedPtr = std::shared_ptr<Illuminance>;

    std_msgs::msg::Header header;
    double illuminance{0.0};
    double variance{0.0};
};
}  // namespace msg
}  // namespace sensor_msgs
#endif  // FAKE_ILLUMINANCE_HPP
