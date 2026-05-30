#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <pm.h>
#include <ros_node_test_assert.hpp>

struct pm_dev
{
    pm_config config{};
    bool started{false};
    uint32_t start_freq{0};
    bool switch_enabled{false};
};

static int g_alloc_count = 0;
static int g_free_count = 0;
static int g_switch_set_count = 0;
static int g_get_state_rc = 0;
static bool g_alloc_fail = false;
static bool g_init_fail = false;
static bool g_start_fail = false;

extern "C" {
struct pm_dev * pm_alloc_adc(const char * name, const char * adc_dev, float scale)
{
    (void)scale;
    if (g_alloc_fail || name == nullptr || adc_dev == nullptr) {
        return nullptr;
    }
    ++g_alloc_count;
    return new pm_dev();
}

struct pm_dev * pm_alloc_digital(const char * name, const char * protocol, const char * dev_path, uint32_t addr)
{
    (void)protocol;
    (void)addr;
    return pm_alloc_adc(name, dev_path, 1.0F);
}

struct pm_dev * pm_alloc_generic(const char * name, const char * charger_node, const char * capacity_node, void * args)
{
    (void)args;
    if (g_alloc_fail || name == nullptr || charger_node == nullptr || capacity_node == nullptr) {
        return nullptr;
    }
    ++g_alloc_count;
    return new pm_dev();
}

int pm_init(struct pm_dev * dev, const struct pm_config * cfg)
{
    if (dev == nullptr || cfg == nullptr || g_init_fail) {
        return -1;
    }
    dev->config = *cfg;
    return 0;
}

void pm_set_callback(struct pm_dev * dev, pm_callback_t cb, void * ctx)
{
    (void)dev;
    (void)cb;
    (void)ctx;
}

int pm_start(struct pm_dev * dev, uint32_t freq_hz)
{
    if (dev == nullptr || g_start_fail) {
        return -1;
    }
    dev->started = true;
    dev->start_freq = freq_hz;
    return 0;
}

int pm_get_state(struct pm_dev * dev, struct pm_state * out_state)
{
    if (dev == nullptr || out_state == nullptr) {
        return -1;
    }
    if (g_get_state_rc != 0) {
        return g_get_state_rc;
    }
    out_state->status = dev->switch_enabled ? PM_STATUS_CHARGING : PM_STATUS_DISCHARGING;
    out_state->percentage = 66.0F;
    out_state->health = 98.0F;
    out_state->cycle_count = 7;
    out_state->error_code = 0;
    out_state->cell_count = 2;
    out_state->cell_voltages[0] = 3.9F;
    out_state->cell_voltages[1] = 3.8F;
    return 0;
}

void pm_free(struct pm_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}

int pm_switch_set(struct pm_dev * dev, const char * channel_name, bool enable)
{
    if (dev == nullptr || channel_name == nullptr) {
        return -1;
    }
    if (std::string(channel_name) != "main") {
        return -2;
    }
    ++g_switch_set_count;
    dev->switch_enabled = enable;
    return 0;
}

bool pm_switch_get(struct pm_dev * dev, const char * channel_name)
{
    if (dev == nullptr || channel_name == nullptr) {
        return false;
    }
    return dev->switch_enabled && std::string(channel_name) == "main";
}
}

#define main pm_node_main
#define private public
#include "../src/pm_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_alloc_count = 0;
    g_free_count = 0;
    g_switch_set_count = 0;
    g_get_state_rc = 0;
    g_alloc_fail = false;
    g_init_fail = false;
    g_start_fail = false;
    g_failures = 0;
}

static void configure_valid()
{
    rclcpp::test::set_parameter("driver", "generic");
    rclcpp::test::set_parameter("name", "main_batt");
    rclcpp::test::set_parameter("frame_id", "test_power");
    rclcpp::test::set_parameter("poll_hz", 2.5);
    rclcpp::test::set_parameter("switch_channels", std::vector<std::string>{"main"});
}

static void test_functional()
{
    reset_fake();
    configure_valid();
    {
        PmNode node;
        CHECK_INT_EQ(g_alloc_count, 1);
        CHECK_INT_EQ(node.status_pub_->messages.size(), 1);
        CHECK_INT_EQ(node.status_pub_->messages.back().status, 1);
        CHECK_FLOAT_EQ(node.status_pub_->messages.back().percentage, 66.0);
        CHECK_INT_EQ(node.status_pub_->messages.back().cell_count, 2);

        peripherals_pm_node::srv::SetPowerSwitch::Request req;
        req.channel = 0;
        req.enable = true;
        auto resp = node.switch_srv_->call(req);
        CHECK_TRUE(resp.success);
        CHECK_INT_EQ(g_switch_set_count, 1);

        node.timer_->trigger();
        CHECK_INT_EQ(node.status_pub_->messages.back().status, 2);
    }
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("driver", "bad");
    try {
        PmNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "driver must be one of: generic, adc, ina219");
    }

    reset_fake();
    configure_valid();
    PmNode node;
    peripherals_pm_node::srv::SetPowerSwitch::Request req;
    req.channel = 1;
    req.enable = true;
    auto resp = node.switch_srv_->call(req);
    CHECK_FALSE(resp.success);
    CHECK_TRUE(resp.message.find("not configured") != std::string::npos);

    const auto before = node.status_pub_->messages.size();
    g_get_state_rc = -5;
    node.timer_->trigger();
    CHECK_INT_EQ(node.status_pub_->messages.size(), before);
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("pm ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("pm ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("pm ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("pm ros error paths test") != 0) {
            return 1;
        }
        std::printf("pm ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
