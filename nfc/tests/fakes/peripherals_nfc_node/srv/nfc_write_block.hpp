#ifndef FAKE_NFC_WRITE_BLOCK_HPP  // NOLINT(build/header_guard)
#define FAKE_NFC_WRITE_BLOCK_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace peripherals_nfc_node
{
namespace srv
{
struct NfcWriteBlock
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t block_addr{0};
        std::vector<uint8_t> data;
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
    };
};
}  // namespace srv
}  // namespace peripherals_nfc_node
#endif  // FAKE_NFC_WRITE_BLOCK_HPP
