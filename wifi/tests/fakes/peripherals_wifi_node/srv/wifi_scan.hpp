#ifndef FAKE_WIFI_SCAN_HPP  // NOLINT(build/header_guard)
#define FAKE_WIFI_SCAN_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <peripherals_wifi_node/msg/wifi_scan_result.hpp>

namespace peripherals_wifi_node
{
namespace srv
{
struct WifiScan
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        std::string ssid;
        uint32_t max_results{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
        uint32_t total_results{0};
        std::vector<msg::WifiScanResult> results;
    };
};
}  // namespace srv
}  // namespace peripherals_wifi_node
#endif  // FAKE_WIFI_SCAN_HPP
