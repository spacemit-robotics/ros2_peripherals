#ifndef FAKE_NFC_READ_BLOCK_HPP  // NOLINT(build/header_guard)
#define FAKE_NFC_READ_BLOCK_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace peripherals_nfc_node
{
namespace srv
{
struct NfcReadBlock
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t block_addr{0};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
        std::vector<uint8_t> data;
    };
};
}  // namespace srv
}  // namespace peripherals_nfc_node
#endif  // FAKE_NFC_READ_BLOCK_HPP
