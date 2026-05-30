#ifndef FAKE_TRIGGER_HPP  // NOLINT(build/header_guard)
#define FAKE_TRIGGER_HPP  // NOLINT(build/header_guard)

#include <memory>
#include <string>

namespace std_srvs
{
namespace srv
{
struct Trigger
{
    struct Request
    {
        using SharedPtr = std::shared_ptr<Request>;
    };

    struct Response
    {
        using SharedPtr = std::shared_ptr<Response>;
        bool success{false};
        std::string message;
    };
};
}  // namespace srv
}  // namespace std_srvs
#endif  // FAKE_TRIGGER_HPP
