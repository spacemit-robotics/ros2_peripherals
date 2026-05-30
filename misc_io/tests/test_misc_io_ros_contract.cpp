#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <misc_io.h>
#include <ros_node_test_assert.hpp>

struct misc_dev
{
    enum misc_type type{MISC_TYPE_GENERIC};
    enum misc_dir dir{MISC_DIR_INPUT};
    enum misc_logic logic{MISC_ACTIVE_HIGH};
    uint16_t debounce_ms{0};
    bool active{false};
    misc_cb_t cb{nullptr};
    void * cb_arg{nullptr};
};

static std::vector<misc_dev *> g_ios;
static int g_set_count = 0;
static int g_free_count = 0;

extern "C" {
struct misc_dev * misc_io_alloc(enum misc_type type, enum misc_dir dir, void * hw_ctx)
{
    auto * ctx = static_cast<misc_gpiod_ctx *>(hw_ctx);
    if (ctx == nullptr || ctx->chip_name == nullptr || std::string(ctx->chip_name) == "badchip") {
        return nullptr;
    }
    auto * dev = new misc_dev();
    dev->type = type;
    dev->dir = dir;
    g_ios.push_back(dev);
    return dev;
}

void misc_io_config(struct misc_dev * dev, enum misc_logic active_logic, uint16_t debounce_ms)
{
    if (dev != nullptr) {
        dev->logic = active_logic;
        dev->debounce_ms = debounce_ms;
    }
}

int misc_io_set(struct misc_dev * dev, bool active)
{
    if (dev == nullptr) {
        return -22;
    }
    if (dev->dir != MISC_DIR_OUTPUT) {
        return -1;
    }
    ++g_set_count;
    dev->active = active;
    return 0;
}

int misc_io_get(struct misc_dev * dev)
{
    if (dev == nullptr) {
        return -22;
    }
    return dev->active ? 1 : 0;
}

void misc_io_trigger(struct misc_dev * dev, misc_cb_t cb, void * args)
{
    if (dev != nullptr) {
        dev->cb = cb;
        dev->cb_arg = args;
    }
}

void misc_io_free(struct misc_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}
}

#define main misc_io_node_main
#define private public
#include "../src/misc_io_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_ios.clear();
    g_set_count = 0;
    g_free_count = 0;
    g_failures = 0;
}

static void configure_two_ios()
{
    rclcpp::test::set_parameter("io_ids", std::vector<int64_t>{1, 2});
    rclcpp::test::set_parameter(
        "types", std::vector<int64_t>{MISC_TYPE_RELAY, MISC_TYPE_SWITCH});
    rclcpp::test::set_parameter(
        "dirs", std::vector<int64_t>{MISC_DIR_OUTPUT, MISC_DIR_INPUT});
    rclcpp::test::set_parameter(
        "active_logics", std::vector<int64_t>{MISC_ACTIVE_HIGH, MISC_ACTIVE_HIGH});
    rclcpp::test::set_parameter("debounce_mss", std::vector<int64_t>{0, 5});
    rclcpp::test::set_parameter(
        "chip_names", std::vector<std::string>{"gpiochip0", "gpiochip0"});
    rclcpp::test::set_parameter("line_offsets", std::vector<int64_t>{3, 4});
    rclcpp::test::set_parameter(
        "consumers", std::vector<std::string>{"out", "in"});
    rclcpp::test::set_parameter(
        "io_names", std::vector<std::string>{"relay_out", "switch_in"});
}

static void test_functional()
{
    reset_fake();
    configure_two_ios();

    {
        MiscIoNode node;
        CHECK_INT_EQ(g_ios.size(), 2);
        CHECK_INT_EQ(node.state_pub_->messages.size(), 2);

        peripherals_misc_io_node::msg::MiscIoCommand cmd;
        cmd.io_id = 1;
        cmd.active = true;
        node.cmd_sub_->receive(cmd);
        CHECK_INT_EQ(g_set_count, 1);
        CHECK_TRUE(g_ios[0]->active);
        CHECK_TRUE(node.state_pub_->messages.back().active);
        CHECK_INT_EQ(node.state_pub_->messages.back().io_id, 1);

        CHECK_TRUE(g_ios[1]->cb != nullptr);
        g_ios[1]->cb(g_ios[1], MISC_EV_ACTIVE, 1234000, g_ios[1]->cb_arg);
        node.state_timer_->trigger();
        CHECK_INT_EQ(node.event_pub_->messages.size(), 1);
        CHECK_INT_EQ(node.event_pub_->messages.back().io_id, 2);
        CHECK_INT_EQ(node.event_pub_->messages.back().event, MISC_EV_ACTIVE);
    }

    CHECK_INT_EQ(g_free_count, 2);
}

static void test_error_paths()
{
    reset_fake();
    rclcpp::test::set_parameter("io_ids", std::vector<int64_t>{1, 1});
    rclcpp::test::set_parameter("types", std::vector<int64_t>{MISC_TYPE_RELAY, MISC_TYPE_RELAY});
    rclcpp::test::set_parameter("dirs", std::vector<int64_t>{MISC_DIR_OUTPUT, MISC_DIR_OUTPUT});
    rclcpp::test::set_parameter(
        "active_logics", std::vector<int64_t>{MISC_ACTIVE_HIGH, MISC_ACTIVE_HIGH});
    rclcpp::test::set_parameter("debounce_mss", std::vector<int64_t>{0, 0});
    rclcpp::test::set_parameter("chip_names", std::vector<std::string>{"gpiochip0", "gpiochip0"});
    rclcpp::test::set_parameter("line_offsets", std::vector<int64_t>{3, 4});
    try {
        MiscIoNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "io_ids must be unique");
    }

    reset_fake();
    rclcpp::test::set_parameter("chip_names", std::vector<std::string>{"badchip"});
    try {
        MiscIoNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::exception & e) {
        CHECK_TRUE(std::string(e.what()).find("misc_io_alloc failed") != std::string::npos);
    }

    reset_fake();
    configure_two_ios();
    MiscIoNode node;
    peripherals_misc_io_node::msg::MiscIoCommand cmd;
    cmd.io_id = 2;
    cmd.active = true;
    node.cmd_sub_->receive(cmd);
    CHECK_INT_EQ(g_set_count, 0);
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("misc_io ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("misc_io ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("misc_io ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("misc_io ros error paths test") != 0) {
            return 1;
        }
        std::printf("misc_io ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
