#ifndef FAKE_WIFI_SCAN_RESULTS_HPP  // NOLINT(build/header_guard)
#define FAKE_WIFI_SCAN_RESULTS_HPP  // NOLINT(build/header_guard)

#include <memory>
#include <vector>

#include <peripherals_wifi_node/msg/wifi_scan_result.hpp>
#include <std_msgs/msg/header.hpp>

namespace peripherals_wifi_node
{
namespace msg
{
struct WifiScanResults
{
    using SharedPtr = std::shared_ptr<WifiScanResults>;

    std_msgs::msg::Header header;
    std::vector<WifiScanResult> results;
};
}  // namespace msg
}  // namespace peripherals_wifi_node
#endif  // FAKE_WIFI_SCAN_RESULTS_HPP
