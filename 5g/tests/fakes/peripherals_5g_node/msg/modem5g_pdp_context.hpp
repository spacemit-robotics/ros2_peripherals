#ifndef FAKE_MODEM5G_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_PDP_CONTEXT_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <string>

namespace peripherals_5g_node
{
namespace msg
{
struct Modem5gPdpContext
{
    uint8_t cid{0};
    uint8_t pdp_type{0};
    std::string apn;
    std::string username;
    bool has_password{false};
};
}  // namespace msg
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_PDP_CONTEXT_HPP
