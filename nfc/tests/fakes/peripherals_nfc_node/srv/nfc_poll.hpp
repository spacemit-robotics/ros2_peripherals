#ifndef FAKE_NFC_POLL_HPP  // NOLINT(build/header_guard)
#define FAKE_NFC_POLL_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

#include <peripherals_nfc_node/msg/nfc_tag_info.hpp>

namespace peripherals_nfc_node
{
namespace srv
{
struct NfcPoll
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint32_t timeout_ms{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
        msg::NfcTagInfo tag_info;
    };
};
}  // namespace srv
}  // namespace peripherals_nfc_node
#endif  // FAKE_NFC_POLL_HPP
