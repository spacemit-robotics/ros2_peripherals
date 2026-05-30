#ifndef FAKE_ROS_NODE_TEST_ASSERT_HPP  // NOLINT(build/header_guard)
#define FAKE_ROS_NODE_TEST_ASSERT_HPP  // NOLINT(build/header_guard)

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        std::printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        ++g_failures; \
    } \
} while (0)

#define CHECK_FALSE(expr) CHECK_TRUE(!(expr))

#define CHECK_INT_EQ(actual, expected) do { \
    const long long actual_value = static_cast<long long>(actual); \
    const long long expected_value = static_cast<long long>(expected); \
    if (actual_value != expected_value) { \
        std::printf("FAIL:%s:%d: expected %s == %lld, got %lld\n", \
            __FILE__, __LINE__, #actual, expected_value, actual_value); \
        ++g_failures; \
    } \
} while (0)

#define CHECK_FLOAT_EQ(actual, expected) do { \
    const double actual_value = static_cast<double>(actual); \
    const double expected_value = static_cast<double>(expected); \
    if (std::fabs(actual_value - expected_value) > 0.001) { \
        std::printf("FAIL:%s:%d: expected %s == %.3f, got %.3f\n", \
            __FILE__, __LINE__, #actual, expected_value, actual_value); \
        ++g_failures; \
    } \
} while (0)

#define CHECK_STR_EQ(actual, expected) do { \
    const std::string actual_value = (actual); \
    const std::string expected_value = (expected); \
    if (actual_value != expected_value) { \
        std::printf("FAIL:%s:%d: expected %s == '%s', got '%s'\n", \
            __FILE__, __LINE__, #actual, expected_value.c_str(), actual_value.c_str()); \
        ++g_failures; \
    } \
} while (0)

static int finish_test(const char * name)
{
    if (g_failures != 0) {
        std::printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    std::printf("%s PASSED\n", name);
    return 0;
}
#endif  // FAKE_ROS_NODE_TEST_ASSERT_HPP
