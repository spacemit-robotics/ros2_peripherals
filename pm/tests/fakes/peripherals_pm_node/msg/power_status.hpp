#ifndef FAKE_POWER_STATUS_HPP  // NOLINT(build/header_guard)
#define FAKE_POWER_STATUS_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <vector>

#include <std_msgs/msg/header.hpp>

namespace peripherals_pm_node
{
namespace msg
{
struct PowerStatus
{
    using SharedPtr = std::shared_ptr<PowerStatus>;

    std_msgs::msg::Header header;
    uint8_t status{0};
    float percentage{0.0F};
    float health{0.0F};
    uint32_t cycle_count{0};
    uint32_t error_code{0};
    uint8_t cell_count{0};
    std::vector<float> cell_voltages;
};
}  // namespace msg
}  // namespace peripherals_pm_node
#endif  // FAKE_POWER_STATUS_HPP
