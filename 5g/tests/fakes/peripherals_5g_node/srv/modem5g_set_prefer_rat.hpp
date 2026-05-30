#ifndef FAKE_MODEM5G_SET_PREFER_RAT_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_SET_PREFER_RAT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gSetPreferRat
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t rat{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        uint8_t rat{0};
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_SET_PREFER_RAT_HPP
