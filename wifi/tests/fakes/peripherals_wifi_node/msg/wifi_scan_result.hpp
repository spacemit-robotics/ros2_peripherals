#ifndef FAKE_WIFI_SCAN_RESULT_HPP  // NOLINT(build/header_guard)
#define FAKE_WIFI_SCAN_RESULT_HPP  // NOLINT(build/header_guard)

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_wifi_node
{
namespace msg
{
struct WifiScanResult
{
    using SharedPtr = std::shared_ptr<WifiScanResult>;

    std::array<uint8_t, 6> bssid{};
    std::string ssid;
    uint32_t freq{0};
    int32_t rssi_dbm{0};
    uint32_t secure{0};
};
}  // namespace msg
}  // namespace peripherals_wifi_node
#endif  // FAKE_WIFI_SCAN_RESULT_HPP
