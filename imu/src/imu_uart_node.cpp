/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

// IMU DDS Publisher Node - Compatible with ros2 run

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>
#include <string>

#include <MyImuPublisher.h>

extern "C" {
#include "imu.h"
}

// Signal handler for graceful shutdown
static std::atomic<bool> g_running(true);

static void signalHandler(int signum)
{
    (void)signum;
    g_running = false;
}

class ImuUartNode
{
public:
    ImuUartNode()
        : imu_dev_(nullptr)
        , sample_rate_(50)
    {
    }

    ~ImuUartNode()
    {
        if (imu_dev_ != nullptr) {
            imu_free(imu_dev_);
        }
    }

    bool init(
        const char* imu_name,
        const char* dev_path,
        uint32_t baud,
        uint32_t rate,
        const std::string& topic_name,
        int domain_id)
    {
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        sample_rate_ = rate;

        imu_dev_ = imu_alloc_uart(imu_name, dev_path, baud, nullptr);
        if (imu_dev_ == nullptr) {
            std::cerr << "Failed to allocate IMU device" << std::endl;
            return false;
        }

        struct imu_config cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.sample_rate = sample_rate_;
        cfg.mounting_matrix[0] = 1; cfg.mounting_matrix[1] = 0; cfg.mounting_matrix[2] = 0;
        cfg.mounting_matrix[3] = 0; cfg.mounting_matrix[4] = 1; cfg.mounting_matrix[5] = 0;
        cfg.mounting_matrix[6] = 0; cfg.mounting_matrix[7] = 0; cfg.mounting_matrix[8] = 1;

        if (imu_init(imu_dev_, &cfg) != 0) {
            std::cerr << "Failed to initialize IMU" << std::endl;
            imu_free(imu_dev_);
            imu_dev_ = nullptr;
            return false;
        }

        std::cout << "IMU device initialized: " << imu_name << " @ " << dev_path
                << " @ " << baud << " baud" << std::endl;

        if (!imu_publisher_.init(topic_name, domain_id)) {
            std::cerr << "Failed to initialize DDS ImuPublisher" << std::endl;
            return false;
        }

        std::cout << "IMU Publisher initialized. Topic: " << topic_name << std::endl;
        return true;
    }

    void run()
    {
        std::cout << "Publishing IMU data at " << sample_rate_ << "Hz. Press Ctrl+C to stop." << std::endl;

        my_sensor_msgs::msg::Imu imu_msg;
        struct imu_data data;
        uint32_t msg_count = 0;

        // Calculate period from sample rate
        const auto period = std::chrono::milliseconds(1000 / sample_rate_);
        auto next_time = std::chrono::steady_clock::now();

        while (g_running) {
            // Read data from real IMU
            int ret = imu_read(imu_dev_, &data);
            if (ret != 0) {
                std::cerr << "IMU read error" << std::endl;
                next_time += period;
                std::this_thread::sleep_until(next_time);
                continue;
            }
            // Get current timestamp
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);

            // Set Header
            imu_msg.header().stamp().sec(static_cast<int32_t>(seconds.count()));
            imu_msg.header().stamp().nanosec(static_cast<uint32_t>(nanoseconds.count()));
            imu_msg.header().frame_id("imu_link");

            // Set orientation from quaternion (w, x, y, z)
            imu_msg.orientation().w(data.quat[0]);
            imu_msg.orientation().x(data.quat[1]);
            imu_msg.orientation().y(data.quat[2]);
            imu_msg.orientation().z(data.quat[3]);

            // Set angular velocity (rad/s)
            imu_msg.angular_velocity().x(data.gyro[0]);
            imu_msg.angular_velocity().y(data.gyro[1]);
            imu_msg.angular_velocity().z(data.gyro[2]);

            // Set linear acceleration (m/s^2)
            imu_msg.linear_acceleration().x(data.acc[0]);
            imu_msg.linear_acceleration().y(data.acc[1]);
            imu_msg.linear_acceleration().z(data.acc[2]);

            // Publish message
            imu_publisher_.writer_->write(&imu_msg);
            msg_count++;

            // Print status every second
            if (msg_count % sample_rate_ == 0) {
                std::cout << "[" << msg_count << "] Published IMU data. "
                    << "Accel: (" << data.acc[0] << ", "
                    << data.acc[1] << ", " << data.acc[2] << ") "
                    << "Gyro: (" << data.gyro[0] << ", "
                    << data.gyro[1] << ", " << data.gyro[2] << ")"
                    << std::endl;
            }

            // Precise 50Hz timing
            next_time += period;
            std::this_thread::sleep_until(next_time);
        }

        std::cout << "\nStopped. Total messages sent: " << msg_count << std::endl;
    }

private:
    ImuPublisher imu_publisher_;
    struct imu_dev* imu_dev_;
    uint32_t sample_rate_;
};

int main(int argc, char** argv)
{
    const char* imu_name = "CMP10A:cmp10a_imu";
    const char* dev_path = "/dev/ttyUSB1";
    uint32_t baud = 9600;
    uint32_t rate = 50;
    std::string topic_name = "rt/imu";
    int domain_id = 0;
    int opt;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "n:d:b:r:t:i:h")) != -1) {
        switch (opt) {
        case 'n':
            imu_name = optarg;
            break;
        case 'd':
            dev_path = optarg;
            break;
        case 'b':
            baud = static_cast<uint32_t>(atoi(optarg));
            break;
        case 'r':
            rate = static_cast<uint32_t>(atoi(optarg));
            if (rate == 0 || rate > 1000) {
                std::cerr << "Invalid rate: " << optarg << " (valid: 1-1000)" << std::endl;
                return 1;
            }
            break;
        case 't':
            topic_name = optarg;
            break;
        case 'i':
            domain_id = atoi(optarg);
            break;
        case 'h':
        default:
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -n <name>     IMU name (default: CMP10A:cmp10a_imu)" << std::endl;
            std::cout << "  -d <device>   Serial device path (default: /dev/ttyUSB0)" << std::endl;
            std::cout << "  -b <baud>     Baud rate (default: 9600)" << std::endl;
            std::cout << "  -r <rate>     Publish rate in Hz (default: 50, range: 1-1000)" << std::endl;
            std::cout << "  -t <topic>    DDS topic name (default: rt/imu)" << std::endl;
            std::cout << "  -i <id>       DDS domain ID (default: 0)" << std::endl;
            std::cout << "  -h            Show this help" << std::endl;
            return (opt == 'h') ? 0 : 1;
        }
    }

    std::cout << "=== IMU UART DDS Publisher ===" << std::endl;
    std::cout << "IMU: " << imu_name << ", Device: " << dev_path
            << ", Baud: " << baud << ", Rate: " << rate << "Hz"
            << ", Topic: " << topic_name << ", Domain: " << domain_id << std::endl;

    ImuUartNode node;

    if (node.init(imu_name, dev_path, baud, rate, topic_name, domain_id)) {
        node.run();
    } else {
        std::cerr << "Failed to initialize IMU UART node!" << std::endl;
        return 1;
    }

    return 0;
}
