#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <nfc.h>
#include <ros_node_test_assert.hpp>

struct nfc_dev
{
    bool initialized{false};
    uint8_t memory[8][16]{};
};

static int g_alloc_count = 0;
static int g_free_count = 0;
static int g_poll_rc = 0;
static bool g_init_fail = false;

extern "C" {
struct nfc_dev * nfc_alloc_i2c(const char * name, const char * i2c_dev, uint8_t addr)
{
    (void)addr;
    if (name == nullptr || i2c_dev == nullptr || std::string(i2c_dev) == "bad") {
        return nullptr;
    }
    auto * dev = new nfc_dev();
    for (uint8_t block = 0; block < 8; ++block) {
        for (uint8_t offset = 0; offset < 16; ++offset) {
            dev->memory[block][offset] = static_cast<uint8_t>(block * 16 + offset);
        }
    }
    ++g_alloc_count;
    return dev;
}

struct nfc_dev * nfc_alloc_spi(const char * name, const char * spi_dev, uint32_t cs_pin)
{
    (void)cs_pin;
    return nfc_alloc_i2c(name, spi_dev, 0);
}

struct nfc_dev * nfc_alloc_uart(const char * name, const char * uart_dev, uint32_t baud)
{
    (void)baud;
    return nfc_alloc_i2c(name, uart_dev, 0);
}

int nfc_init(struct nfc_dev * dev)
{
    if (dev == nullptr || g_init_fail) {
        return -1;
    }
    dev->initialized = true;
    return 0;
}

void nfc_set_callback(struct nfc_dev * dev, nfc_event_cb_t cb, void * ctx)
{
    (void)dev;
    (void)cb;
    (void)ctx;
}

void nfc_free(struct nfc_dev * dev)
{
    if (dev != nullptr) {
        ++g_free_count;
        delete dev;
    }
}

int nfc_poll(struct nfc_dev * dev, struct nfc_tag_info * info, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (dev == nullptr || info == nullptr || !dev->initialized) {
        return -1;
    }
    if (g_poll_rc != 0) {
        return g_poll_rc;
    }
    std::memset(info, 0, sizeof(*info));
    const uint8_t uid[] = {0xDE, 0xAD, 0xBE, 0xEF};
    std::memcpy(info->uid, uid, sizeof(uid));
    info->uid_len = sizeof(uid);
    info->type = NFC_TAG_MIFARE_CLASSIC;
    info->rssi_dbm = -42;
    info->ats[0] = 0x75;
    info->ats_len = 1;
    return 0;
}

int nfc_read_block(struct nfc_dev * dev, uint8_t block, uint8_t * buf, size_t len)
{
    if (dev == nullptr || buf == nullptr || len != 16 || block >= 8) {
        return -22;
    }
    std::memcpy(buf, dev->memory[block], 16);
    return 16;
}

int nfc_write_block(struct nfc_dev * dev, uint8_t block, const uint8_t * buf, size_t len)
{
    if (dev == nullptr || buf == nullptr || len != 16 || block >= 8) {
        return -22;
    }
    std::memcpy(dev->memory[block], buf, 16);
    return 16;
}
}

#define main nfc_node_main
#define private public
#include "../src/nfc_node.cpp"
#undef private
#undef main

static void reset_fake()
{
    rclcpp::test::clear_parameters();
    g_alloc_count = 0;
    g_free_count = 0;
    g_poll_rc = 0;
    g_init_fail = false;
    g_failures = 0;
}

static void configure_valid()
{
    rclcpp::test::set_parameter("transport", "i2c");
    rclcpp::test::set_parameter("driver", "MOCK");
    rclcpp::test::set_parameter("name", "nfc0");
    rclcpp::test::set_parameter("device", "mock://nfc");
    rclcpp::test::set_parameter("i2c_addr", 0x28);
    rclcpp::test::set_parameter("auto_poll_enabled", false);
}

static void test_functional()
{
    reset_fake();
    configure_valid();
    {
        NfcNode node;
        auto poll = node.poll_srv_->call(peripherals_nfc_node::srv::NfcPoll::Request{25});
        CHECK_TRUE(poll.success);
        CHECK_STR_EQ(poll.message, "tag detected");
        CHECK_INT_EQ(poll.tag_info.uid_len, 4);
        CHECK_INT_EQ(poll.tag_info.uid[0], 0xDE);
        CHECK_INT_EQ(node.tag_pub_->messages.size(), 1);

        peripherals_nfc_node::srv::NfcReadBlock::Request read_req;
        read_req.block_addr = 3;
        auto read = node.read_srv_->call(read_req);
        CHECK_TRUE(read.success);
        CHECK_INT_EQ(read.data.size(), 16);
        CHECK_INT_EQ(read.data[0], 48);

        peripherals_nfc_node::srv::NfcWriteBlock::Request write_req;
        write_req.block_addr = 3;
        write_req.data.assign(16, 0xA5);
        auto write = node.write_srv_->call(write_req);
        CHECK_TRUE(write.success);
        read = node.read_srv_->call(read_req);
        CHECK_INT_EQ(read.data[0], 0xA5);
    }
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_error_paths()
{
    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("transport", "bad");
    try {
        NfcNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::invalid_argument & e) {
        CHECK_STR_EQ(e.what(), "transport must be one of: i2c, spi, uart");
    }

    reset_fake();
    configure_valid();
    rclcpp::test::set_parameter("device", "bad");
    try {
        NfcNode node;
        (void)node;
        CHECK_TRUE(false);
    } catch (const std::runtime_error & e) {
        CHECK_STR_EQ(e.what(), "nfc_alloc_* failed");
    }

    reset_fake();
    configure_valid();
    NfcNode node;
    peripherals_nfc_node::srv::NfcWriteBlock::Request bad_write;
    bad_write.block_addr = 1;
    bad_write.data.assign(15, 0);
    auto write = node.write_srv_->call(bad_write);
    CHECK_FALSE(write.success);
    CHECK_STR_EQ(write.message, "write failed: data must contain exactly 16 bytes");

    g_poll_rc = 1;
    auto poll = node.poll_srv_->call(peripherals_nfc_node::srv::NfcPoll::Request{5});
    CHECK_FALSE(poll.success);
    CHECK_STR_EQ(poll.message, "no tag");
}

int main(int argc, char ** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "functional") {
        test_functional();
        return finish_test("nfc ros functional test");
    }
    if (mode == "error-paths") {
        test_error_paths();
        return finish_test("nfc ros error paths test");
    }
    if (mode == "all") {
        test_functional();
        if (finish_test("nfc ros functional test") != 0) {
            return 1;
        }
        test_error_paths();
        if (finish_test("nfc ros error paths test") != 0) {
            return 1;
        }
        std::printf("nfc ros contract test PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
