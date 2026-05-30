#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <key.h>
#include <ros_node_test_assert.hpp>

struct key_handle
{
    key_config_t config{};
    key_callback_t cb{nullptr};
    void * user_data{nullptr};
};

static std::vector<key_handle *> g_keys;
static int g_service_start_count = 0;
static int g_service_stop_count = 0;
static int g_remove_count = 0;

extern "C" {
int key_service_start(void)
{
    ++g_service_start_count;
    return 0;
}

void key_service_stop(void)
{
    ++g_service_stop_count;
}

struct key_handle * key_add_gpio(const key_config_t * config, key_callback_t cb, void * user_data)
{
    if (config == nullptr || cb == nullptr || config->gpio_num <= 0) {
        return nullptr;
    }
    auto * handle = new key_handle();
    handle->config = *config;
    handle->cb = cb;
    handle->user_data = user_data;
    g_keys.push_back(handle);
    return handle;
}

void key_remove(struct key_handle * key)
{
    if (key != nullptr) {
        ++g_remove_count;
        delete key;
    }
}
}

#define main key_node_main
#define private public
#include "../src/key_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_keys.clear();
    g_service_start_count = 0;
    g_service_stop_count = 0;
    g_remove_count = 0;
    g_failures = 0;
}

static void test_functional()
{
    reset_fake();
    rclcpp::test::set_parameter("event_topic", "test/key/events");
    rclcpp::test::set_parameter("frame_id", "test_key");
    rclcpp::test::set_parameter("publish_period_ms", int64_t{5});
    rclcpp::test::set_parameter("key_ids", std::vector<int64_t>{42});
    rclcpp::test::set_parameter("gpio_nums", std::vector<int64_t>{11});
    rclcpp::test::set_parameter("active_lows", std::vector<int64_t>{1});
    rclcpp::test::set_parameter("long_press_ms", std::vector<int64_t>{100});
    rclcpp::test::set_parameter("double_click_ms", std::vector<int64_t>{50});
    rclcpp::test::set_parameter("key_names", std::vector<std::string>{"user_key"});

    {
        KeyNode node;
        CHECK_INT_EQ(g_service_start_count, 1);
        CHECK_INT_EQ(g_keys.size(), 1);
        CHECK_INT_EQ(g_keys[0]->config.gpio_num, 11);
        CHECK_INT_EQ(g_keys[0]->config.active_low, 1);
        CHECK_INT_EQ(g_keys[0]->config.long_press_ms, 100);
        CHECK_INT_EQ(g_keys[0]->config.double_click_ms, 50);

        g_keys[0]->cb(g_keys[0], KEY_EV_PRESSED, g_keys[0]->user_data);
        g_keys[0]->cb(g_keys[0], KEY_EV_DOUBLE_CLICK, g_keys[0]->user_data);
        node.drain_timer_->trigger();

        CHECK_INT_EQ(node.publisher_->messages.size(), 2);
        CHECK_INT_EQ(node.publisher_->messages[0].key_id, 42);
        CHECK_INT_EQ(node.publisher_->messages[0].event_type, 1);
        CHECK_STR_EQ(node.publisher_->messages[0].header.frame_id, "test_key");
        CHECK_INT_EQ(node.publisher_->messages[1].event_type, 4);
    }

    CHECK_INT_EQ(g_remove_count, 1);
    CHECK_INT_EQ(g_service_stop_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    rclcpp::test::set_parameter("key_ids", std::vector<int64_t>{1, 2});
    rclcpp::test::set_parameter("gpio_nums", std::vector<int64_t>{10});
    rclcpp::test::set_parameter("active_lows", std::vector<int64_t>{1, 1});
    rclcpp::test::set_parameter("long_press_ms", std::vector<int64_t>{100, 100});
    rclcpp::test::set_parameter("double_click_ms", std::vector<int64_t>{50, 50});
    try {
        KeyNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "gpio_nums size mismatch with key_ids");
    }

    reset_fake();
    rclcpp::test::set_parameter("gpio_nums", std::vector<int64_t>{0});
    try {
        KeyNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "gpio_nums must be > 0");
    }
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("key ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("key ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("key ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("key ros error paths test") != 0) {
            return 1;
        }
        std::printf("key ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
