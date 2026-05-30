#ifndef FAKE_MODEM5G_SET_FLIGHT_MODE_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_SET_FLIGHT_MODE_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gSetFlightMode
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        bool enable{false};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        bool flight_mode_enabled{false};
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_SET_FLIGHT_MODE_HPP
