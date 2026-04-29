/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MYIMUPUBLISHER_H
#define MYIMUPUBLISHER_H

#include <string>
#include <sensor_msgs/msg/imu.hpp>
#include "ImuPublisher.h"
#include "Imu.h"


class MyImuPublisher{
    public:
        MyImuPublisher(std::string topic_name, int domain_id){
            // 查看topic_name 是否以rt/开头
            if (topic_name.find("rt/") != 0){
                // 查看是否以/开头
                if (topic_name.find_first_of("/") == std::string::npos || topic_name.find_first_of("/") != 0){
                    // 添加rt/前缀
                    topic_name = "rt/" + topic_name;
                }else{
                    // 添加rt/前缀
                    topic_name = "rt" + topic_name;
                }
            }
            imu_pub_.init(topic_name, domain_id);
        }
        void publish(const sensor_msgs::msg::Imu& imu_msg){
            auto dds_msg = my_sensor_msgs::msg::Imu();
            dds_msg.header().stamp().sec() = imu_msg.header.stamp.sec;
            dds_msg.header().stamp().nanosec() = imu_msg.header.stamp.nanosec;
            dds_msg.header().frame_id() = imu_msg.header.frame_id;
            dds_msg.orientation().w() = imu_msg.orientation.w;
            dds_msg.orientation().x() = imu_msg.orientation.x;
            dds_msg.orientation().y() = imu_msg.orientation.y;
            dds_msg.orientation().z() = imu_msg.orientation.z;
            memcpy(dds_msg.orientation_covariance().data(),
                imu_msg.orientation_covariance.data(),
                9 * sizeof(double));
            dds_msg.angular_velocity().x() = imu_msg.angular_velocity.x;
            dds_msg.angular_velocity().y() = imu_msg.angular_velocity.y;
            dds_msg.angular_velocity().z() = imu_msg.angular_velocity.z;
            memcpy(dds_msg.angular_velocity_covariance().data(),
                imu_msg.angular_velocity_covariance.data(),
                9 * sizeof(double));
            dds_msg.linear_acceleration().x() = imu_msg.linear_acceleration.x;
            dds_msg.linear_acceleration().y() = imu_msg.linear_acceleration.y;
            dds_msg.linear_acceleration().z() = imu_msg.linear_acceleration.z;
            memcpy(dds_msg.linear_acceleration_covariance().data(),
                imu_msg.linear_acceleration_covariance.data(),
                9 * sizeof(double));
            imu_pub_.writer_->write(&dds_msg);
        }

    private:
        ImuPublisher imu_pub_;
};
#endif
