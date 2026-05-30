#ifndef FAKE_MODEM5G_IP_INFO_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_IP_INFO_HPP  // NOLINT(build/header_guard)

#include <string>

namespace peripherals_5g_node
{
namespace msg
{
struct Modem5gIpInfo
{
    std::string ip;
    std::string gateway;
    std::string dns1;
    std::string dns2;
};
}  // namespace msg
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_IP_INFO_HPP
