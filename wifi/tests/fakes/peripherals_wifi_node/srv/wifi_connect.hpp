#ifndef FAKE_WIFI_CONNECT_HPP  // NOLINT(build/header_guard)
#define FAKE_WIFI_CONNECT_HPP  // NOLINT(build/header_guard)

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_wifi_node
{
namespace srv
{
struct WifiConnect
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        std::string ssid;
        std::string password;
        bool fast_connect{false};
        uint32_t secure{0};
        std::array<uint8_t, 6> bssid{};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
    };
};
}  // namespace srv
}  // namespace peripherals_wifi_node
#endif  // FAKE_WIFI_CONNECT_HPP
