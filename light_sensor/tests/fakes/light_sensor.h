/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


struct light_sensor_dev;

int light_sensor_init(struct light_sensor_dev *dev);
int light_sensor_poll(struct light_sensor_dev *dev, uint32_t *light_value);
void light_sensor_free(struct light_sensor_dev *dev);

struct light_sensor_dev *light_sensor_alloc_i2c(const char *name, const char *i2c_dev, uint8_t addr);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* LIGHT_SENSOR_H */
