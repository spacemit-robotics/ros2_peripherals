/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MYIMUSUBSCRIBER_H
#define MYIMUSUBSCRIBER_H
#include <string>
#include "ImuSubscriber.h"

class MyImuSubscriber{
    public:
        MyImuSubscriber(std::string topic_name, int domain_id){
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
            imu_sub_.init(topic_name, domain_id);
        }
        void set_callback(std::function<void(const my_sensor_msgs::msg::Imu&)> callback_func){
            imu_sub_.set_callback(callback_func);
        }

    private:
        ImuSubscriber imu_sub_;
};
#endif
