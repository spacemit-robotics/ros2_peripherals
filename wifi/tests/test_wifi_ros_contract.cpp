#include <algorithm>
#include <array>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <vector>
#include <string>

#include <ros_node_test_assert.hpp>
#include <wifi.h>

static bool g_initialized = false;
static bool g_enabled = false;
static bool g_connected = false;
static enum wifi_status g_init_status = WIFI_STATUS_SUCCESS;
static enum wifi_status g_on_status = WIFI_STATUS_SUCCESS;
static enum wifi_status g_scan_status = WIFI_STATUS_SUCCESS;
static int g_connect_count = 0;
static std::string g_last_ssid;
static std::string g_last_password;

extern "C" {
enum wifi_status wifi_init(void)
{
    if (g_init_status != WIFI_STATUS_SUCCESS) {
        return g_init_status;
    }
    g_initialized = true;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_deinit(void)
{
    g_initialized = false;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_on(enum wifi_mode mode)
{
    if (!g_initialized) {
        return WIFI_STATUS_NOT_READY;
    }
    if (g_on_status != WIFI_STATUS_SUCCESS) {
        return g_on_status;
    }
    if (mode == WIFI_MODE_STATION) {
        g_enabled = true;
        g_connected = true;
    }
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_off(enum wifi_mode mode)
{
    (void)mode;
    g_enabled = false;
    g_connected = false;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_connect(struct wifi_sta_connect_param * param)
{
    if (param == nullptr || param->ssid == nullptr || std::strlen(param->ssid) == 0) {
        return WIFI_STATUS_INVALID;
    }
    ++g_connect_count;
    g_last_ssid = param->ssid;
    g_last_password = param->password == nullptr ? "" : param->password;
    g_connected = true;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_disconnect(void)
{
    g_connected = false;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_auto_reconnect(bool enable)
{
    (void)enable;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_auto_connect(const char * ssid)
{
    (void)ssid;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_get_info(struct wifi_sta_info * info)
{
    if (info == nullptr) {
        return WIFI_STATUS_INVALID;
    }
    std::memset(info, 0, sizeof(*info));
    std::strcpy(info->ssid, "HomeNet");
    info->rssi = -47;
    info->sec = WIFI_SEC_WPA2_PSK;
    info->bssid[0] = 0xAA;
    info->ip_addr[0] = 192;
    info->ip_addr[1] = 168;
    info->ip_addr[2] = 1;
    info->ip_addr[3] = 99;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_sta_list_networks(struct wifi_sta_list * list)
{
    (void)list;
    return WIFI_STATUS_UNSUPPORTED;
}

enum wifi_status wifi_sta_remove_networks(const char * ssid)
{
    (void)ssid;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_ap_enable(struct wifi_ap_config * config)
{
    (void)config;
    return WIFI_STATUS_UNSUPPORTED;
}

enum wifi_status wifi_ap_disable(void)
{
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_ap_get_config(struct wifi_ap_config * config)
{
    (void)config;
    return WIFI_STATUS_UNSUPPORTED;
}

enum wifi_status wifi_register_msg_cb(wifi_msg_cb_t msg_cb, void * arg)
{
    (void)msg_cb;
    (void)arg;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_set_scan_param(struct wifi_scan_param * param)
{
    (void)param;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_get_scan_results(
    struct wifi_scan_result * result, const char * ssid, uint32_t * bss_num, uint32_t arr_size)
{
    if (result == nullptr || bss_num == nullptr) {
        return WIFI_STATUS_INVALID;
    }
    if (g_scan_status != WIFI_STATUS_SUCCESS) {
        return g_scan_status;
    }
    if (ssid != nullptr && std::string(ssid) == "missing") {
        *bss_num = 0;
        return WIFI_STATUS_SUCCESS;
    }

    *bss_num = 2;
    const uint32_t stored = std::min<uint32_t>(arr_size, 2);
    for (uint32_t i = 0; i < stored; ++i) {
        std::memset(&result[i], 0, sizeof(result[i]));
        std::strcpy(result[i].ssid, i == 0 ? "HomeNet" : "LabNet");
        result[i].freq = i == 0 ? 2412 : 5180;
        result[i].rssi = i == 0 ? -45 : -55;
        result[i].key_mgmt = WIFI_SEC_WPA2_PSK;
        result[i].bssid[0] = static_cast<uint8_t>(0xAA + i);
    }
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_set_mac(const char * ifname, const uint8_t * mac_addr)
{
    (void)ifname;
    (void)mac_addr;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_get_mac(const char * ifname, uint8_t * mac_addr)
{
    (void)ifname;
    if (mac_addr == nullptr) {
        return WIFI_STATUS_INVALID;
    }
    mac_addr[0] = 0x12;
    return WIFI_STATUS_SUCCESS;
}

enum wifi_status wifi_linkd_protocol(enum wifi_linkd_mode mode, wifi_msg_cb_t cb, void * params, int second)
{
    (void)mode;
    (void)cb;
    (void)params;
    (void)second;
    return WIFI_STATUS_UNSUPPORTED;
}

enum wifi_status wifi_get_state(struct wifi_state * state)
{
    if (!g_initialized) {
        return WIFI_STATUS_NOT_READY;
    }
    if (state == nullptr) {
        return WIFI_STATUS_INVALID;
    }
    std::memset(state, 0, sizeof(*state));
    state->current_mode = WIFI_MODE_STATION;
    state->current_mode_enable_flag = g_enabled ? 1 : 0;
    state->sta_state = g_connected ? WIFI_STA_NET_CONNECTED : WIFI_STA_DISCONNECTED;
    return WIFI_STATUS_SUCCESS;
}
}

#define main wifi_node_main
#define private public
#include "../src/wifi_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_initialized = false;
    g_enabled = false;
    g_connected = false;
    g_init_status = WIFI_STATUS_SUCCESS;
    g_on_status = WIFI_STATUS_SUCCESS;
    g_scan_status = WIFI_STATUS_SUCCESS;
    g_connect_count = 0;
    g_last_ssid.clear();
    g_last_password.clear();
    g_failures = 0;
}

static void configure_valid()
{
    rclcpp::test::set_parameter("frame_id", "test_wifi");
    rclcpp::test::set_parameter("enable_on_startup", true);
    rclcpp::test::set_parameter("auto_reconnect", false);
    rclcpp::test::set_parameter("status_period_ms", 100);
    rclcpp::test::set_parameter("scan_period_ms", 0);
    rclcpp::test::set_parameter("scan_max_results", 32);
}

static void test_functional()
{
    reset_fake();
    configure_valid();
    WifiNode node;
    CHECK_INT_EQ(node.status_pub_->messages.size(), 1);
    CHECK_TRUE(node.status_pub_->messages.back().connected);
    CHECK_STR_EQ(node.status_pub_->messages.back().ssid, "HomeNet");
    CHECK_INT_EQ(node.status_pub_->messages.back().bssid[0], 0xAA);
    CHECK_INT_EQ(node.status_pub_->messages.back().ip_addr[0], 192);

    peripherals_wifi_node::srv::WifiScan::Request scan_req;
    scan_req.max_results = 1;
    auto scan_resp = node.scan_srv_->call(scan_req);
    CHECK_TRUE(scan_resp.success);
    CHECK_INT_EQ(scan_resp.total_results, 2);
    CHECK_INT_EQ(scan_resp.results.size(), 1);
    CHECK_STR_EQ(scan_resp.message, "ok (results truncated by max_results)");
    CHECK_INT_EQ(node.scan_pub_->messages.back().results.size(), 1);

    peripherals_wifi_node::srv::WifiConnect::Request connect_req;
    connect_req.ssid = "Office";
    connect_req.password = "secret";
    connect_req.bssid[0] = 1;
    auto connect_resp = node.connect_srv_->call(connect_req);
    CHECK_TRUE(connect_resp.success);
    CHECK_STR_EQ(g_last_ssid, "Office");
    CHECK_STR_EQ(g_last_password, "secret");

    auto disconnect_resp = node.disconnect_srv_->call();
    CHECK_TRUE(disconnect_resp.success);
    CHECK_STR_EQ(disconnect_resp.message, "ok");
}

static void test_error_paths()
{
    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("scan_max_results", 0);
    try {
        WifiNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "scan_max_results must be in [1, 256]");
    }

    reset_fake();
    configure_valid();
    g_init_status = WIFI_STATUS_FAIL;
    try {
        WifiNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::runtime_error & e) {
        CHECK_TRUE(std::string(e.what()).find("wifi_init failed") != std::string::npos);
    }

    reset_fake();
    configure_valid();
    WifiNode node;
    peripherals_wifi_node::srv::WifiConnect::Request connect_req;
    auto connect_resp = node.connect_srv_->call(connect_req);
    CHECK_FALSE(connect_resp.success);
    CHECK_STR_EQ(connect_resp.message, "ssid must not be empty");
    CHECK_INT_EQ(g_connect_count, 0);

    g_scan_status = WIFI_STATUS_TIMEOUT;
    peripherals_wifi_node::srv::WifiScan::Request scan_req;
    auto scan_resp = node.scan_srv_->call(scan_req);
    CHECK_FALSE(scan_resp.success);
    CHECK_TRUE(scan_resp.message.find("scan failed: timeout") != std::string::npos);
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("wifi ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("wifi ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("wifi ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("wifi ros error paths test") != 0) {
            return 1;
        }
        std::printf("wifi ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
