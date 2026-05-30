#ifndef FAKE_MODEM5G_DATA_CALL_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_DATA_CALL_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

namespace peripherals_5g_node
{
namespace srv
{
struct Modem5gDataCall
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
        uint8_t cid{0};
        bool start{false};
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        int32_t status_code{0};
        std::string message;
        uint8_t cid{0};
        uint8_t data_state{0};
    };
};
}  // namespace srv
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_DATA_CALL_HPP
