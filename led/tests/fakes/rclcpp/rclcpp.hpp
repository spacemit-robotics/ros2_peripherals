#ifndef FAKE_RCLCPP_HPP  // NOLINT(build/header_guard)
#define FAKE_RCLCPP_HPP  // NOLINT(build/header_guard)

#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace rclcpp
{
class Time
{
public:
    Time() = default;
    Time(int64_t seconds, uint32_t nanoseconds, int clock_type = 0)
        : nanoseconds_(seconds * 1000000000LL + static_cast<int64_t>(nanoseconds)),
            clock_type_(clock_type)
    {
    }

    int64_t nanoseconds() const { return nanoseconds_; }
    int get_clock_type() const { return clock_type_; }

private:
    int64_t nanoseconds_{0};
    int clock_type_{0};
};

class Logger
{
};

class Clock
{
public:
    int get_clock_type() const { return 0; }
};

namespace test
{
using ParameterValue = std::variant<
    bool,
    int,
    int64_t,
    double,
    std::string,
    std::vector<int64_t>,
    std::vector<std::string>>;

inline std::unordered_map<std::string, ParameterValue> & parameters()
{
    static std::unordered_map<std::string, ParameterValue> values;
    return values;
}

inline void clear_parameters()
{
    parameters().clear();
}

template<typename T>
void set_parameter(const std::string & name, T value)
{
    parameters()[name] = ParameterValue(std::move(value));
}

inline void set_parameter(const std::string & name, const char * value)
{
    parameters()[name] = std::string(value);
}

template<typename T>
T value_as(const ParameterValue & value)
{
    if (const auto * typed = std::get_if<T>(&value)) {
        return *typed;
    }
    if constexpr (std::is_same_v<T, int>) {
        if (const auto * typed = std::get_if<int64_t>(&value)) {
            return static_cast<int>(*typed);
        }
    }
    if constexpr (std::is_same_v<T, int64_t>) {
        if (const auto * typed = std::get_if<int>(&value)) {
            return static_cast<int64_t>(*typed);
        }
    }
    if constexpr (std::is_same_v<T, double>) {
        if (const auto * typed = std::get_if<int>(&value)) {
            return static_cast<double>(*typed);
        }
        if (const auto * typed = std::get_if<int64_t>(&value)) {
            return static_cast<double>(*typed);
        }
    }
    throw std::runtime_error("parameter type mismatch");
}
}  // namespace test

class Parameter
{
public:
    Parameter() = default;
    explicit Parameter(test::ParameterValue value) : value_(std::move(value)) {}

    std::vector<int64_t> as_integer_array() const
    {
        return test::value_as<std::vector<int64_t>>(value_);
    }

    std::vector<std::string> as_string_array() const
    {
        return test::value_as<std::vector<std::string>>(value_);
    }

private:
    test::ParameterValue value_{false};
};

struct KeepLast
{
    explicit KeepLast(size_t depth_in) : depth(depth_in) {}
    size_t depth;
};

class QoS
{
public:
    explicit QoS(size_t depth) : depth_(depth) {}
    explicit QoS(KeepLast keep_last) : depth_(keep_last.depth) {}

    QoS & reliable() { return *this; }
    QoS & transient_local() { return *this; }

private:
    size_t depth_;
};

class SensorDataQoS : public QoS
{
public:
    SensorDataQoS() : QoS(10) {}
};

template<typename MsgT>
class Publisher
{
public:
    using SharedPtr = std::shared_ptr<Publisher<MsgT>>;

    explicit Publisher(std::string topic) : topic_(std::move(topic)) {}

    void publish(const MsgT & msg)
    {
        messages.push_back(msg);
    }

    const char * get_topic_name() const
    {
        return topic_.c_str();
    }

    std::string topic_;
    std::vector<MsgT> messages;
};

template<typename MsgT>
class Subscription
{
public:
    using SharedPtr = std::shared_ptr<Subscription<MsgT>>;
    using Callback = std::function<void(typename MsgT::SharedPtr)>;

    template<typename CallbackT>
    Subscription(std::string topic, CallbackT cb) : topic_(std::move(topic)), callback_(cb)
    {
    }

    void receive(const MsgT & msg)
    {
        callback_(std::make_shared<MsgT>(msg));
    }

    std::string topic_;
    Callback callback_;
};

template<typename ServiceT>
class Service
{
public:
    using SharedPtr = std::shared_ptr<Service<ServiceT>>;
    using Request = typename ServiceT::Request;
    using Response = typename ServiceT::Response;
    using Callback = std::function<void(std::shared_ptr<Request>, std::shared_ptr<Response>)>;

    template<typename CallbackT>
    Service(std::string name, CallbackT cb) : name_(std::move(name)), callback_(cb)
    {
    }

    Response call(const Request & request = Request())
    {
        auto req = std::make_shared<Request>(request);
        auto resp = std::make_shared<Response>();
        callback_(req, resp);
        return *resp;
    }

    std::string name_;
    Callback callback_;
};

class TimerBase
{
public:
    using SharedPtr = std::shared_ptr<TimerBase>;

    explicit TimerBase(std::function<void()> callback) : callback_(std::move(callback)) {}

    void trigger()
    {
        if (callback_) {
            callback_();
        }
    }

    void reset()
    {
        callback_ = nullptr;
    }

private:
    std::function<void()> callback_;
};

class Node
{
public:
    explicit Node(std::string name) : name_(std::move(name)) {}
    virtual ~Node() = default;

    template<typename T>
    T declare_parameter(const std::string & name, const T & default_value)
    {
        auto & params = test::parameters();
        const auto it = params.find(name);
        if (it != params.end()) {
            return test::value_as<T>(it->second);
        }
        params[name] = default_value;
        return default_value;
    }

    Parameter get_parameter(const std::string & name) const
    {
        const auto it = test::parameters().find(name);
        if (it == test::parameters().end()) {
            throw std::runtime_error("parameter not declared: " + name);
        }
        return Parameter(it->second);
    }

    template<typename MsgT, typename QosT>
    typename Publisher<MsgT>::SharedPtr create_publisher(const std::string & topic, const QosT &)
    {
        return std::make_shared<Publisher<MsgT>>(topic);
    }

    template<typename MsgT, typename CallbackT>
    typename Subscription<MsgT>::SharedPtr create_subscription(
        const std::string & topic, const QoS &, CallbackT cb)
    {
        return std::make_shared<Subscription<MsgT>>(topic, cb);
    }

    template<typename ServiceT, typename CallbackT>
    typename Service<ServiceT>::SharedPtr create_service(const std::string & name, CallbackT cb)
    {
        return std::make_shared<Service<ServiceT>>(name, cb);
    }

    template<typename RepT, typename PeriodT, typename CallbackT>
    TimerBase::SharedPtr create_wall_timer(std::chrono::duration<RepT, PeriodT>, CallbackT cb)
    {
        return std::make_shared<TimerBase>(std::function<void()>(cb));
    }

    Time now() const
    {
        static int64_t next = 1;
        return Time(next++, 0);
    }

    Logger get_logger() const { return Logger(); }

    std::shared_ptr<Clock> get_clock() const
    {
        static auto clock = std::make_shared<Clock>();
        return clock;
    }

private:
    std::string name_;
};

inline void init(int, char **)
{
}

inline void shutdown()
{
}

template<typename NodeT>
void spin(std::shared_ptr<NodeT>)
{
}
}  // namespace rclcpp

#define RCLCPP_INFO(...) do {} while (0)
#define RCLCPP_WARN(...) do {} while (0)
#define RCLCPP_WARN_THROTTLE(...) do {} while (0)
#define RCLCPP_INFO_STREAM(...) do {} while (0)
#endif  // FAKE_RCLCPP_HPP
