#ifndef FAKE_MODEM5G_GET_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_GET_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

#include <peripherals_5g_node/msg/modem5g_pdp_context.hpp>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gGetPdpContext
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t cid{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        msg::Modem5gPdpContext context;
        std::string password;
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_GET_PDP_CONTEXT_HPP
