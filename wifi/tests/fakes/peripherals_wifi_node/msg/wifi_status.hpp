#ifndef FAKE_WIFI_STATUS_HPP  // NOLINT(build/header_guard)
#define FAKE_WIFI_STATUS_HPP  // NOLINT(build/header_guard)

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <std_msgs/msg/header.hpp>

namespace peripherals_wifi_node
{
namespace msg
{
struct WifiStatus
{
    using SharedPtr = std::shared_ptr<WifiStatus>;

    std_msgs::msg::Header header;
    uint8_t mode{0};
    bool connected{false};
    std::string ssid;
    int32_t rssi_dbm{0};
    uint32_t secure{0};
    std::array<uint8_t, 6> bssid{};
    std::array<uint8_t, 4> ip_addr{};
};
}  // namespace msg
}  // namespace peripherals_wifi_node
#endif  // FAKE_WIFI_STATUS_HPP
