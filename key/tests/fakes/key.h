/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef KEY_H
#define KEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_EV_PRESSED,
    KEY_EV_RELEASED,
    KEY_EV_CLICK,       /* 单击 */
    KEY_EV_DOUBLE_CLICK,/* 双击 */
    KEY_EV_LONG_PRESS,  /* 长按触发 */
    KEY_EV_HOLD_REPEAT  /* 长按连发 */
} key_event_t;

/* --- 不透明句柄 --- */
struct key_handle;

/* --- 回调函数原型 --- */
typedef void (*key_callback_t)(struct key_handle *key, key_event_t event, void *user_data);

/* --- 配置参数 --- */
typedef struct {
    int gpio_num;           // Linux GPIO 编号 (如 23 代表 GPIO23)
    int active_low;         // 1:低电平有效(按下为0), 0:高电平有效

    /* 时间配置 (单位ms)，填 0 则使用默认值 */
    int long_press_ms;      // 默认 1500
    int double_click_ms;    // 默认 300
} key_config_t;

/* ============================================================
 * API 接口
 * ============================================================ */

/**
 * @brief 全局初始化 (启动后台扫描线程)
 * @return 0 成功, -1 失败
 */
int key_service_start(void);

/**
 * @brief 全局销毁 (停止线程，清理资源)
 */
void key_service_stop(void);

/**
 * @brief 添加一个 GPIO 按键
 * @param config 按键的硬件和参数配置
 * @param cb 事件触发时的回调函数
 * @param user_data 用户私有数据(会透传给回调)
 * @return 句柄指针, 失败返回 NULL
 */
struct key_handle *key_add_gpio(const key_config_t *config, key_callback_t cb, void *user_data);

/**
 * @brief 删除一个按键
 */
void key_remove(struct key_handle *key);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
