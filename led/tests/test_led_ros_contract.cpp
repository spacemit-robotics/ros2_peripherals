#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ros_node_test_assert.hpp>
#include <led.h>

struct led_dev
{
    std::string name;
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t brightness{0};
    bool on{false};
    uint16_t blink_period_ms{0};
    uint16_t blink_on_ms{0};
    uint8_t blink_count{0};
    uint16_t breath_period_ms{0};
};

static std::vector<led_dev *> g_leds;
static int g_alloc_generic_count = 0;
static int g_alloc_spi_count = 0;
static int g_free_count = 0;
static int g_tick_count = 0;

extern "C" {
struct led_dev * led_alloc_generic(const char * name, void * args)
{
    (void)args;
    if (name == nullptr || std::strlen(name) == 0 || std::strstr(name, "fail") != nullptr) {
        return nullptr;
    }
    auto * dev = new led_dev();
    dev->name = name;
    g_leds.push_back(dev);
    ++g_alloc_generic_count;
    return dev;
}

struct led_dev * led_alloc_spi(const char * name, void * args)
{
    (void)args;
    if (name == nullptr || std::strlen(name) == 0) {
        return nullptr;
    }
    auto * dev = new led_dev();
    dev->name = name;
    g_leds.push_back(dev);
    ++g_alloc_spi_count;
    return dev;
}

void led_set_state(struct led_dev * dev, bool on)
{
    if (dev != nullptr) {
        dev->on = on;
        if (!on) {
            dev->brightness = 0;
        }
    }
}

void led_toggle(struct led_dev * dev)
{
    if (dev != nullptr) {
        dev->on = !dev->on;
    }
}

void led_set_brightness(struct led_dev * dev, uint8_t brightness)
{
    if (dev != nullptr) {
        dev->brightness = brightness;
        dev->on = brightness > 0;
    }
}

void led_set_color(struct led_dev * dev, const struct led_color * color)
{
    if (dev != nullptr && color != nullptr) {
        dev->r = color->r;
        dev->g = color->g;
        dev->b = color->b;
    }
}

void led_blink(struct led_dev * dev, const struct led_blink_param * param)
{
    if (dev != nullptr && param != nullptr) {
        dev->blink_period_ms = param->period_ms;
        dev->blink_on_ms = param->on_ms;
        dev->blink_count = param->count;
        dev->on = param->on_ms > 0;
        dev->brightness = dev->on ? 255 : 0;
    }
}

void led_breath(struct led_dev * dev, uint16_t period_ms)
{
    if (dev != nullptr) {
        dev->breath_period_ms = period_ms;
    }
}

void led_tick(struct led_dev * dev, uint16_t dt_ms)
{
    (void)dev;
    (void)dt_ms;
    ++g_tick_count;
}

void led_free(struct led_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}
}

#define main led_node_main
#define private public
#include "../src/led_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_leds.clear();
    g_alloc_generic_count = 0;
    g_alloc_spi_count = 0;
    g_free_count = 0;
    g_tick_count = 0;
    g_failures = 0;
}

static void test_functional()
{
    reset_fake();
    rclcpp::test::set_parameter("led_ids", std::vector<int64_t>{7});
    rclcpp::test::set_parameter("transports", std::vector<std::string>{"generic"});
    rclcpp::test::set_parameter("names", std::vector<std::string>{"MOCK:status"});
    rclcpp::test::set_parameter("generic_sysfs_names", std::vector<std::string>{""});
    rclcpp::test::set_parameter("generic_active_levels", std::vector<int64_t>{0});
    rclcpp::test::set_parameter("spi_dev_paths", std::vector<std::string>{"/dev/null"});
    rclcpp::test::set_parameter("spi_num_leds", std::vector<int64_t>{1});
    rclcpp::test::set_parameter("spi_speed_hz", std::vector<int64_t>{6400000});
    rclcpp::test::set_parameter("spi_reset_bytes", std::vector<int64_t>{80});
    rclcpp::test::set_parameter("tick_period_ms", int64_t{25});
    rclcpp::test::set_parameter("publish_period_ms", int64_t{100});

    {
        LedNode node;
        CHECK_INT_EQ(g_alloc_generic_count, 1);
        CHECK_INT_EQ(node.state_pub_->messages.size(), 1);
        CHECK_INT_EQ(node.state_pub_->messages.back().led_id, 7);

        peripherals_led_node::msg::LedCommand cmd;
        cmd.led_id = 7;
        cmd.r = 10;
        cmd.g = 20;
        cmd.b = 30;
        cmd.brightness = 128;
        cmd.mode = 0;
        node.cmd_sub_->receive(cmd);

        CHECK_INT_EQ(node.state_pub_->messages.size(), 2);
        const auto state = node.state_pub_->messages.back();
        CHECK_INT_EQ(state.led_id, 7);
        CHECK_INT_EQ(state.r, 10);
        CHECK_INT_EQ(state.g, 20);
        CHECK_INT_EQ(state.b, 30);
        CHECK_INT_EQ(state.brightness, 128);
        CHECK_TRUE(state.is_on);
        CHECK_INT_EQ(g_leds[0]->brightness, 128);

        cmd.mode = 1;
        cmd.period_ms = 100;
        cmd.on_ms = 40;
        cmd.count = 1;
        node.cmd_sub_->receive(cmd);
        CHECK_INT_EQ(node.state_pub_->messages.back().mode, 1);
        CHECK_INT_EQ(g_leds[0]->blink_period_ms, 100);

        node.tick_timer_->trigger();
        CHECK_INT_EQ(g_tick_count, 1);
    }

    CHECK_INT_EQ(g_free_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    rclcpp::test::set_parameter("led_ids", std::vector<int64_t>{1, 1});
    rclcpp::test::set_parameter("transports", std::vector<std::string>{"generic", "generic"});
    rclcpp::test::set_parameter("names", std::vector<std::string>{"MOCK:a", "MOCK:b"});
    rclcpp::test::set_parameter("generic_sysfs_names", std::vector<std::string>{"", ""});
    rclcpp::test::set_parameter("generic_active_levels", std::vector<int64_t>{0, 0});
    rclcpp::test::set_parameter("spi_dev_paths", std::vector<std::string>{"/dev/null", "/dev/null"});
    rclcpp::test::set_parameter("spi_num_leds", std::vector<int64_t>{1, 1});
    rclcpp::test::set_parameter("spi_speed_hz", std::vector<int64_t>{6400000, 6400000});
    rclcpp::test::set_parameter("spi_reset_bytes", std::vector<int64_t>{80, 80});
    try {
        LedNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "led_ids must be unique");
    }

    reset_fake();
    rclcpp::test::set_parameter("transports", std::vector<std::string>{"generic"});
    rclcpp::test::set_parameter("names", std::vector<std::string>{"fail"});
    try {
        LedNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::runtime_error & e) {
        CHECK_TRUE(std::string(e.what()).find("failed to allocate LED device") != std::string::npos);
    }
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("led ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("led ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("led ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("led ros error paths test") != 0) {
            return 1;
        }
        std::printf("led ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
