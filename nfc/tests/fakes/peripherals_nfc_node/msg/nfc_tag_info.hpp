#ifndef FAKE_NFC_TAG_INFO_HPP  // NOLINT(build/header_guard)
#define FAKE_NFC_TAG_INFO_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <vector>

#include <std_msgs/msg/header.hpp>

namespace peripherals_nfc_node
{
namespace msg
{
struct NfcTagInfo
{
    using SharedPtr = std::shared_ptr<NfcTagInfo>;

    std_msgs::msg::Header header;
    std::vector<uint8_t> uid;
    uint8_t uid_len{0};
    uint8_t tag_type{0};
    int8_t rssi_dbm{0};
    std::vector<uint8_t> ats;
    uint8_t ats_len{0};
};
}  // namespace msg
}  // namespace peripherals_nfc_node
#endif  // FAKE_NFC_TAG_INFO_HPP
