#ifndef FAKE_SET_POWER_SWITCH_HPP  // NOLINT(build/header_guard)
#define FAKE_SET_POWER_SWITCH_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_pm_node
{
namespace srv
{
struct SetPowerSwitch
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t channel{0};
        bool enable{false};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
    };
};
}  // namespace srv
}  // namespace peripherals_pm_node
#endif  // FAKE_SET_POWER_SWITCH_HPP
