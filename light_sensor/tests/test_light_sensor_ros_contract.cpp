#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include <light_sensor.h>
#include <ros_node_test_assert.hpp>

struct light_sensor_dev
{
    uint32_t next_lux{123};
};

static int g_alloc_count = 0;
static int g_init_count = 0;
static int g_free_count = 0;
static int g_poll_rc = 0;
static bool g_init_fail = false;

extern "C" {
struct light_sensor_dev * light_sensor_alloc_i2c(const char * name, const char * i2c_dev, uint8_t addr)
{
    (void)addr;
    if (name == nullptr || i2c_dev == nullptr || std::string(i2c_dev) == "bad") {
        return nullptr;
    }
    ++g_alloc_count;
    return new light_sensor_dev();
}

int light_sensor_init(struct light_sensor_dev * dev)
{
    if (dev == nullptr || g_init_fail) {
        return -1;
    }
    ++g_init_count;
    return 0;
}

int light_sensor_poll(struct light_sensor_dev * dev, uint32_t * light_value)
{
    if (dev == nullptr || light_value == nullptr) {
        return -1;
    }
    if (g_poll_rc != 0) {
        return g_poll_rc;
    }
    *light_value = dev->next_lux;
    dev->next_lux += 11;
    return 0;
}

void light_sensor_free(struct light_sensor_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}
}

#define main light_sensor_node_main
#define private public
#include "../src/light_sensor_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_alloc_count = 0;
    g_init_count = 0;
    g_free_count = 0;
    g_poll_rc = 0;
    g_init_fail = false;
    g_failures = 0;
}

static void configure_valid()
{
    rclcpp::test::set_parameter("driver", "MOCK");
    rclcpp::test::set_parameter("name", "als0");
    rclcpp::test::set_parameter("device", "mock://als");
    rclcpp::test::set_parameter("i2c_addr", 0x48);
    rclcpp::test::set_parameter("frame_id", "test_light");
    rclcpp::test::set_parameter("topic_name", "test/light");
    rclcpp::test::set_parameter("poll_hz", 10.0);
    rclcpp::test::set_parameter("variance", 0.25);
}

static void test_functional()
{
    reset_fake();
    configure_valid();
    {
        LightSensorNode node;
        CHECK_INT_EQ(g_alloc_count, 1);
        CHECK_INT_EQ(g_init_count, 1);

        node.timer_->trigger();
        node.timer_->trigger();
        CHECK_INT_EQ(node.pub_->messages.size(), 2);
        CHECK_FLOAT_EQ(node.pub_->messages[0].illuminance, 123.0);
        CHECK_FLOAT_EQ(node.pub_->messages[0].variance, 0.25);
        CHECK_STR_EQ(node.pub_->messages[0].header.frame_id, "test_light");
        CHECK_FLOAT_EQ(node.pub_->messages[1].illuminance, 134.0);
    }
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("i2c_addr", 128);
    try {
        LightSensorNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "i2c_addr must be in [0, 127]");
    }

    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("device", "bad");
    try {
        LightSensorNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::runtime_error & e) {
        CHECK_STR_EQ(e.what(), "light_sensor_alloc_i2c failed");
    }

    reset_fake();
    configure_valid();
    g_poll_rc = -11;
    LightSensorNode node;
    node.timer_->trigger();
    CHECK_INT_EQ(node.pub_->messages.size(), 0);
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("light_sensor ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("light_sensor ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("light_sensor ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("light_sensor ros error paths test") != 0) {
            return 1;
        }
        std::printf("light_sensor ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
