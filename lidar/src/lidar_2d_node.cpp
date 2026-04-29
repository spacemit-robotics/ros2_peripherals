/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * LiDAR 2D ROS2 Wrapper Node
 *
 * Generic ROS2 node for 2D LiDAR sensors using the unified lidar interface
 * from components/peripherals/lidar.
 */

// C system headers
extern "C" {
#include <lidar.h>
}

// C++ system headers
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <string>

// Other headers
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_srvs/srv/empty.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD(x) ((x) * M_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / M_PI)

class Lidar2DNode final : public rclcpp::Node {
public:
    Lidar2DNode() : rclcpp::Node("lidar_2d_node") {
        init_params();

        dev_ = alloc_device();
        if (!dev_) {
            throw std::runtime_error("lidar_alloc_* failed");
        }

        lidar_config cfg{};
        cfg.rpm = static_cast<int>(rpm_);
        cfg.angle_min_deg = static_cast<float>(RAD2DEG(angle_min_));
        cfg.angle_max_deg = static_cast<float>(RAD2DEG(angle_max_));
        cfg.range_min_m = static_cast<float>(range_min_);
        cfg.range_max_m = static_cast<float>(range_max_);
        cfg.return_mode = return_mode_;
        cfg.enable_transform = false;

        if (lidar_init(dev_, &cfg) != 0) {
            throw std::runtime_error("lidar_init failed");
        }

        lidar_set_callback(dev_, &Lidar2DNode::on_frame_thunk, this);
        if (lidar_start(dev_) != 0) {
            throw std::runtime_error("lidar_start failed");
        }

        scan_pub_ = create_publisher<sensor_msgs::msg::LaserScan>(
            topic_name_, rclcpp::SensorDataQoS());

        stop_motor_srv_ = create_service<std_srvs::srv::Empty>(
            "stop_motor", std::bind(&Lidar2DNode::stop_motor_callback, this,
                                    std::placeholders::_1, std::placeholders::_2));

        start_motor_srv_ = create_service<std_srvs::srv::Empty>(
            "start_motor", std::bind(&Lidar2DNode::start_motor_callback, this,
                std::placeholders::_1, std::placeholders::_2));

        RCLCPP_INFO(get_logger(), "lidar_2d_node ready: %s model=%s",
                    transport_.c_str(), model_.c_str());
    }

    ~Lidar2DNode() override {
        if (dev_) {
            (void)lidar_stop(dev_);
            lidar_free(dev_);
            dev_ = nullptr;
        }
    }

private:
    void init_params() {
        // Device params
        transport_ = declare_parameter<std::string>("channel_type", "serial");
        model_ = declare_parameter<std::string>("model", "");
        frame_id_ = declare_parameter<std::string>("frame_id", "laser_frame");
        topic_name_ = declare_parameter<std::string>("topic_name", "scan");

        // UART params
        serial_port_ =
            declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
        serial_baudrate_ = declare_parameter<int>("serial_baudrate", 230400);

        // Ethernet params
        tcp_ip_ = declare_parameter<std::string>("tcp_ip", "192.168.0.7");
        tcp_port_ = declare_parameter<int>("tcp_port", 2368);

        // Scan params
        rpm_ = declare_parameter<double>("scan_frequency", 10.0) * 60.0;
        angle_min_ = declare_parameter<double>("angle_min", -M_PI);
        angle_max_ = declare_parameter<double>("angle_max", M_PI);
        range_min_ = declare_parameter<double>("range_min", 0.1);
        range_max_ = declare_parameter<double>("range_max", 30.0);
        return_mode_ = declare_parameter<int>("return_mode", 0);

        inverted_ = declare_parameter<bool>("inverted", false);
        flip_x_axis_ = declare_parameter<bool>("flip_x_axis", false);
        angle_compensate_ = declare_parameter<bool>("angle_compensate", false);
        scan_bins_ = declare_parameter<int>("scan_bins", 720);
    }

    lidar_dev *alloc_device() {
        if (transport_ == "serial" || transport_ == "uart") {
            return lidar_alloc_uart("lidar0", serial_port_.c_str(),
                                    static_cast<uint32_t>(serial_baudrate_),
                                    model_.c_str(), nullptr);
        }
        if (transport_ == "tcp" || transport_ == "ethernet") {
            return lidar_alloc_ethernet("lidar0", tcp_ip_.c_str(),
                                        static_cast<uint16_t>(tcp_port_),
                                        model_.c_str(), nullptr);
        }
        if (transport_ == "sim") {
            return lidar_alloc_sim("lidar0", nullptr);
        }
        throw std::runtime_error("Unsupported channel_type: " + transport_);
    }

    static void on_frame_thunk(struct lidar_dev * /*dev*/,
            const struct lidar_frame *frame, void *ctx) {
        auto *self = static_cast<Lidar2DNode *>(ctx);
        if (self && frame)
            self->publish_scan(*frame);
    }

    void publish_scan(const struct lidar_frame &frame) {
        if (frame.point_count == 0)
            return;

        auto scan_msg = std::make_shared<sensor_msgs::msg::LaserScan>();

        // Set timestamp from frame or use current time
        if (frame.system_stamp_ns > 0) {
            scan_msg->header.stamp.sec =
                static_cast<int32_t>(frame.system_stamp_ns / 1000000000ULL);
            scan_msg->header.stamp.nanosec =
                static_cast<uint32_t>(frame.system_stamp_ns % 1000000000ULL);
        } else {
            scan_msg->header.stamp = now();
        }
        scan_msg->header.frame_id = frame_id_;

        scan_msg->angle_min = static_cast<float>(angle_min_);
        scan_msg->angle_max = static_cast<float>(angle_max_);
        scan_msg->range_min = static_cast<float>(range_min_);
        scan_msg->range_max = static_cast<float>(range_max_);

        // Calculate angle increment based on scan_bins_
        scan_msg->angle_increment = (scan_msg->angle_max - scan_msg->angle_min) /
                                    static_cast<float>(scan_bins_);

        // Estimate scan time based on RPM
        float scan_time = 60.0f / static_cast<float>(rpm_);
        scan_msg->scan_time = scan_time;
        scan_msg->time_increment = scan_time / static_cast<float>(scan_bins_);

        // Initialize ranges and intensities
        scan_msg->ranges.assign(static_cast<size_t>(scan_bins_),
                                std::numeric_limits<float>::infinity());
        scan_msg->intensities.assign(static_cast<size_t>(scan_bins_), 0.0f);

        // Convert point cloud to laser scan
        for (uint32_t i = 0; i < frame.point_count; ++i) {
            const auto &p = frame.points[i];

            // Calculate range and angle from x,y coordinates
            float range = std::sqrt(p.x * p.x + p.y * p.y);
            if (range < scan_msg->range_min || range > scan_msg->range_max)
                continue;

            float angle = std::atan2(p.y, p.x);

            // Apply inversion if needed
            if (inverted_) {
                angle = -angle;
            }

            if (angle < scan_msg->angle_min || angle > scan_msg->angle_max)
                continue;

            // Calculate bin index
            int idx = static_cast<int>((angle - scan_msg->angle_min) /
                scan_msg->angle_increment);

            // Apply flip_x_axis if needed
            if (flip_x_axis_) {
                int midpoint = scan_bins_ / 2;
                if (idx >= midpoint) {
                    idx = idx - midpoint;
                } else {
                    idx = idx + midpoint;
                }
            }

            if (idx >= 0 && idx < scan_bins_) {
                // Keep the closest point for each bin
                if (range < scan_msg->ranges[static_cast<size_t>(idx)]) {
                    scan_msg->ranges[static_cast<size_t>(idx)] = range;
                    scan_msg->intensities[static_cast<size_t>(idx)] = p.intensity;
                }
            }
        }

        scan_pub_->publish(*scan_msg);
    }

    void stop_motor_callback(
        const std::shared_ptr<std_srvs::srv::Empty::Request> /*req*/,
        std::shared_ptr<std_srvs::srv::Empty::Response> /*res*/) {
        RCLCPP_INFO(get_logger(), "Stop motor requested");
        if (dev_) {
            lidar_stop(dev_);
        }
    }

    void start_motor_callback(
        const std::shared_ptr<std_srvs::srv::Empty::Request> /*req*/,
        std::shared_ptr<std_srvs::srv::Empty::Response> /*res*/) {
        RCLCPP_INFO(get_logger(), "Start motor requested");
        if (dev_) {
            lidar_start(dev_);
        }
    }

    // Params
    std::string transport_;
    std::string model_;
    std::string frame_id_;
    std::string topic_name_;
    std::string serial_port_;
    int serial_baudrate_{230400};
    std::string tcp_ip_;
    int tcp_port_{2368};

    double rpm_{600.0};
    double angle_min_{-M_PI};
    double angle_max_{M_PI};
    double range_min_{0.1};
    double range_max_{30.0};
    int return_mode_{0};

    bool inverted_{false};
    bool flip_x_axis_{false};
    bool angle_compensate_{false};
    int scan_bins_{720};

    lidar_dev *dev_{nullptr};

    rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
    rclcpp::Service<std_srvs::srv::Empty>::SharedPtr stop_motor_srv_;
    rclcpp::Service<std_srvs::srv::Empty>::SharedPtr start_motor_srv_;
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<Lidar2DNode>());
    } catch (const std::exception &e) {
        fprintf(stderr, "lidar_2d_node exception: %s\n", e.what());
    }
    rclcpp::shutdown();
    return 0;
}
