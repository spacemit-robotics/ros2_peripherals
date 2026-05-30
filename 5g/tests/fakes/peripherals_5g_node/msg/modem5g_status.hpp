#ifndef FAKE_MODEM5G_STATUS_HPP  // NOLINT(build/header_guard)
#define FAKE_MODEM5G_STATUS_HPP  // NOLINT(build/header_guard)

#include <cstdint>
#include <memory>
#include <string>

#include <peripherals_5g_node/msg/modem5g_ip_info.hpp>
#include <peripherals_5g_node/msg/modem5g_pdp_context.hpp>
#include <std_msgs/msg/header.hpp>

namespace peripherals_5g_node
{
namespace msg
{
struct Modem5gStatus
{
    using SharedPtr = std::shared_ptr<Modem5gStatus>;

    std_msgs::msg::Header header;
    bool snapshot_ok{false};
    int32_t snapshot_status_code{0};
    std::string snapshot_message;
    std::string device_name;
    std::string uart_device;
    uint32_t baud{0};
    bool flight_mode_enabled{false};
    std::string manufacturer;
    std::string model;
    std::string revision;
    std::string imei;
    uint8_t power_state{0};
    uint8_t sim_state{0};
    std::string iccid;
    std::string imsi;
    std::string msisdn;
    uint8_t reg_state{0};
    uint8_t rat{0};
    std::string mcc;
    std::string mnc;
    uint32_t tac{0};
    uint32_t cell_id{0};
    uint16_t pci{0};
    uint32_t arfcn{0};
    std::string band;
    std::string operator_name;
    int16_t rssi_dbm{0};
    int16_t rsrp_dbm{0};
    int16_t rsrq_db{0};
    int16_t sinr_db{0};
    uint8_t data_state{0};
    uint8_t active_cid{0};
    Modem5gPdpContext pdp_context;
    Modem5gIpInfo ip_info;
};
}  // namespace msg
}  // namespace peripherals_5g_node
#endif  // FAKE_MODEM5G_STATUS_HPP
