#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <5g.h>
#include <ros_node_test_assert.hpp>

struct modem_5g_dev
{
    bool initialized{false};
    enum modem_5g_power_state power_state{MODEM_5G_POWER_ON};
    enum modem_5g_data_state data_state{MODEM_5G_DATA_DISCONNECTED};
    struct modem_5g_pdp_context pdp{};
    uint32_t last_at_timeout{0};
};

static int g_alloc_count = 0;
static int g_free_count = 0;
static int g_deinit_count = 0;

extern "C" {
struct modem_5g_dev * modem_5g_alloc_uart(const char * name, const char * uart_dev, uint32_t baud)
{
    (void)baud;
    if (name == nullptr || uart_dev == nullptr || std::string(uart_dev) == "bad") {
        return nullptr;
    }
    auto * dev = new modem_5g_dev();
    dev->pdp.cid = 1;
    dev->pdp.pdp_type = MODEM_5G_PDP_IPV4V6;
    std::strcpy(dev->pdp.apn, "default.apn");
    ++g_alloc_count;
    return dev;
}

enum modem_5g_status modem_5g_init(struct modem_5g_dev * dev)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->initialized = true;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_deinit(struct modem_5g_dev * dev)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->initialized = false;
    ++g_deinit_count;
    return MODEM_5G_STATUS_SUCCESS;
}

void modem_5g_free(struct modem_5g_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}

void modem_5g_set_event_cb(struct modem_5g_dev * dev, modem_5g_event_cb_t cb, void * ctx)
{
    (void)dev;
    (void)cb;
    (void)ctx;
}

enum modem_5g_status modem_5g_power_on(struct modem_5g_dev * dev)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->power_state = MODEM_5G_POWER_ON;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_power_off(struct modem_5g_dev * dev)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->power_state = MODEM_5G_POWER_OFF;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_reset(struct modem_5g_dev * dev)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->power_state = MODEM_5G_POWER_ON;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_set_flight_mode(struct modem_5g_dev * dev, bool enable)
{
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->power_state = enable ? MODEM_5G_POWER_OFF : MODEM_5G_POWER_ON;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_power_state(struct modem_5g_dev * dev, enum modem_5g_power_state * state)
{
    if (dev == nullptr || state == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    *state = dev->power_state;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_basic_info(struct modem_5g_dev * dev, struct modem_5g_basic_info * info)
{
    if (dev == nullptr || info == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    std::memset(info, 0, sizeof(*info));
    std::strcpy(info->manufacturer, "MockVendor");
    std::strcpy(info->model, "MockMR");
    std::strcpy(info->revision, "1.2.3");
    std::strcpy(info->imei, "123456789012345");
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_sim_info(struct modem_5g_dev * dev, struct modem_5g_sim_info * info)
{
    if (dev == nullptr || info == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    std::memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_SIM_READY;
    std::strcpy(info->iccid, "89860000000000000001");
    std::strcpy(info->imsi, "460001234567890");
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_reg_info(struct modem_5g_dev * dev, struct modem_5g_reg_info * info)
{
    if (dev == nullptr || info == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    std::memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_REG_REGISTERED_HOME;
    info->rat = MODEM_5G_RAT_LTE;
    std::strcpy(info->mcc, "460");
    std::strcpy(info->mnc, "001");
    std::strcpy(info->operator_name, "MockCarrier");
    info->cell_id = 42;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_signal_info(struct modem_5g_dev * dev, struct modem_5g_signal_info * info)
{
    if (dev == nullptr || info == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    info->rssi = -51;
    info->rsrp = -82;
    info->rsrq = -9;
    info->sinr = 21;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_set_prefer_rat(struct modem_5g_dev * dev, enum modem_5g_rat rat)
{
    (void)rat;
    return dev == nullptr ? MODEM_5G_STATUS_INVALID : MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_set_pdp_context(
    struct modem_5g_dev * dev, const struct modem_5g_pdp_context * ctx)
{
    if (dev == nullptr || ctx == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->pdp = *ctx;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_pdp_context(
    struct modem_5g_dev * dev, uint8_t cid, struct modem_5g_pdp_context * ctx)
{
    if (dev == nullptr || ctx == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    *ctx = dev->pdp;
    ctx->cid = cid;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_data_start(struct modem_5g_dev * dev, uint8_t cid)
{
    (void)cid;
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->data_state = MODEM_5G_DATA_CONNECTED;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_data_stop(struct modem_5g_dev * dev, uint8_t cid)
{
    (void)cid;
    if (dev == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->data_state = MODEM_5G_DATA_DISCONNECTED;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_data_state(struct modem_5g_dev * dev, enum modem_5g_data_state * state)
{
    if (dev == nullptr || state == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    *state = dev->data_state;
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_get_ip_info(struct modem_5g_dev * dev, uint8_t cid, struct modem_5g_ip_info * info)
{
    (void)cid;
    if (dev == nullptr || info == nullptr) {
        return MODEM_5G_STATUS_INVALID;
    }
    std::strcpy(info->ip, "10.0.0.2");
    std::strcpy(info->gateway, "10.0.0.1");
    std::strcpy(info->dns1, "8.8.8.8");
    return MODEM_5G_STATUS_SUCCESS;
}

enum modem_5g_status modem_5g_send_at(
    struct modem_5g_dev * dev, const char * cmd, char * resp, size_t resp_len, uint32_t timeout_ms)
{
    if (dev == nullptr || cmd == nullptr || resp == nullptr || resp_len == 0) {
        return MODEM_5G_STATUS_INVALID;
    }
    dev->last_at_timeout = timeout_ms;
    std::snprintf(resp, resp_len, "OK:%s:%u", cmd, timeout_ms);
    return MODEM_5G_STATUS_SUCCESS;
}
}

#define main modem_5g_node_main
#define private public
#include "../src/modem_5g_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_alloc_count = 0;
    g_free_count = 0;
    g_deinit_count = 0;
    g_failures = 0;
}

static void configure_valid()
{
    rclcpp::test::set_parameter("name", "MOCK:modem0");
    rclcpp::test::set_parameter("uart_device", "mock://uart");
    rclcpp::test::set_parameter("baud", 115200);
    rclcpp::test::set_parameter("status_period_ms", 100);
    rclcpp::test::set_parameter("at_timeout_ms", 2000);
    rclcpp::test::set_parameter("at_response_max_bytes", 128);
}

static void test_functional()
{
    reset_fake();
    configure_valid();
    {
        Modem5gNode node;
        CHECK_INT_EQ(g_alloc_count, 1);
        CHECK_INT_EQ(node.status_pub_->messages.size(), 1);
        CHECK_TRUE(node.status_pub_->messages.back().snapshot_ok);
        CHECK_STR_EQ(node.status_pub_->messages.back().manufacturer, "MockVendor");
        CHECK_INT_EQ(node.status_pub_->messages.back().sim_state, MODEM_5G_SIM_READY);

        peripherals_5g_node::srv::Modem5gSetPdpContext::Request pdp_req;
        pdp_req.cid = 4;
        pdp_req.pdp_type = MODEM_5G_PDP_IPV4;
        pdp_req.apn = "internet";
        pdp_req.username = "user";
        pdp_req.password = "pass";
        auto pdp_resp = node.set_pdp_srv_->call(pdp_req);
        CHECK_TRUE(pdp_resp.success);
        CHECK_INT_EQ(pdp_resp.context.cid, 4);
        CHECK_STR_EQ(pdp_resp.context.apn, "internet");
        CHECK_TRUE(pdp_resp.context.has_password);

        peripherals_5g_node::srv::Modem5gGetPdpContext::Request get_req;
        get_req.cid = 4;
        auto get_resp = node.get_pdp_srv_->call(get_req);
        CHECK_TRUE(get_resp.success);
        CHECK_STR_EQ(get_resp.password, "pass");

        peripherals_5g_node::srv::Modem5gDataCall::Request data_req;
        data_req.cid = 4;
        data_req.start = true;
        auto data_resp = node.data_call_srv_->call(data_req);
        CHECK_TRUE(data_resp.success);
        CHECK_INT_EQ(data_resp.data_state, MODEM_5G_DATA_CONNECTED);

        peripherals_5g_node::srv::Modem5gSendAt::Request at_req;
        at_req.command = "ATI";
        at_req.timeout_ms = 9;
        auto at_resp = node.send_at_srv_->call(at_req);
        CHECK_TRUE(at_resp.success);
        CHECK_STR_EQ(at_resp.response, "OK:ATI:9");

        peripherals_5g_node::srv::Modem5gSetFlightMode::Request flight_req;
        flight_req.enable = true;
        auto flight_resp = node.flight_mode_srv_->call(flight_req);
        CHECK_TRUE(flight_resp.success);
        CHECK_TRUE(flight_resp.flight_mode_enabled);
    }
    CHECK_INT_EQ(g_deinit_count, 1);
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("default_cid", 21);
    try {
        Modem5gNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "default_cid must be in [1, 20]");
    }

    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("uart_device", "bad");
    try {
        Modem5gNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::runtime_error & e) {
        CHECK_STR_EQ(e.what(), "modem_5g_alloc_uart returned null");
    }

    reset_fake();
    configure_valid();
    Modem5gNode node;

    peripherals_5g_node::srv::Modem5gSetPreferRat::Request rat_req;
    rat_req.rat = 99;
    auto rat_resp = node.prefer_rat_srv_->call(rat_req);
    CHECK_FALSE(rat_resp.success);
    CHECK_INT_EQ(rat_resp.status_code, MODEM_5G_STATUS_INVALID);

    peripherals_5g_node::srv::Modem5gSetPdpContext::Request pdp_req;
    pdp_req.cid = 21;
    pdp_req.pdp_type = MODEM_5G_PDP_IPV4;
    auto pdp_resp = node.set_pdp_srv_->call(pdp_req);
    CHECK_FALSE(pdp_resp.success);
    CHECK_INT_EQ(pdp_resp.status_code, MODEM_5G_STATUS_INVALID);

    pdp_req.cid = 1;
    pdp_req.pdp_type = 99;
    pdp_resp = node.set_pdp_srv_->call(pdp_req);
    CHECK_FALSE(pdp_resp.success);

    pdp_req.pdp_type = MODEM_5G_PDP_IPV4;
    pdp_req.apn.assign(MODEM_5G_APN_MAX_LEN + 1, 'a');
    pdp_resp = node.set_pdp_srv_->call(pdp_req);
    CHECK_FALSE(pdp_resp.success);
    CHECK_STR_EQ(pdp_resp.message, "set_pdp_context: APN/username/password too long");

    peripherals_5g_node::srv::Modem5gSendAt::Request at_req;
    auto at_resp = node.send_at_srv_->call(at_req);
    CHECK_FALSE(at_resp.success);
    CHECK_INT_EQ(at_resp.status_code, MODEM_5G_STATUS_INVALID);
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("5g ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("5g ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("5g ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("5g ros error paths test") != 0) {
            return 1;
        }
        std::printf("5g ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
