#ifndef FAKE_MODEM5G_SEND_AT_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_SEND_AT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gSendAt
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        std::string command;
        uint32_t timeout_ms{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        std::string response;
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_SEND_AT_HPP
