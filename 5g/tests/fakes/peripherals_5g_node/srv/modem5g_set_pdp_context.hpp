#ifndef FAKE_MODEM5G_SET_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_SET_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

#include <peripherals_5g_node/msg/modem5g_pdp_context.hpp>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gSetPdpContext
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t cid{0};
        uint8_t pdp_type{0};
        std::string apn;
        std::string username;
        std::string password;
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        msg::Modem5gPdpContext context;
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_SET_PDP_CONTEXT_HPP
