/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PM_H
#define PM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Power status
 */
enum pm_status {
    PM_STATUS_UNKNOWN = 0,
    PM_STATUS_DISCHARGING,
    PM_STATUS_CHARGING,
    PM_STATUS_FULL,
    PM_STATUS_FAULT,
    PM_STATUS_SLEEP,
};

/*
 * struct pm_state - battery/power state
 * @timestamp_us: sample timestamp
 * @voltage:      total voltage (V)
 * @current:      total current (A), positive=discharge, negative=charge
 * @power:        power consumption (W)
 * @percentage:   state of charge (0.0 ~ 100.0)
 * @temperature:  battery temperature (Celsius)
 * @health:       state of health (0.0 ~ 100.0)
 * @cycle_count:  charge cycle count
 * @status:       charging status
 * @error_code:   hardware error code (0=OK)
 * @cell_count:   number of cells (0 if not available)
 * @cell_voltages: individual cell voltages (max 16S)
 */
struct pm_state {
    uint64_t timestamp_us;
    float voltage;
    float current;
    float power;
    float percentage;
    float temperature;
    float health;
    uint32_t cycle_count;
    enum pm_status status;
    uint32_t error_code;
    uint8_t cell_count;
    float cell_voltages[16];
};

/*
 * struct pm_config - power configuration
 * @capacity_mah: battery design capacity (mAh)
 * @max_voltage:  full charge voltage
 * @min_voltage:  empty voltage
 * @warn_voltage: low voltage warning threshold
 * @crit_voltage: critical shutdown threshold
 * @max_temp:     over-temperature threshold
 */
struct pm_config {
    float capacity_mah;
    float max_voltage;
    float min_voltage;
    float warn_voltage;
    float crit_voltage;
    float max_temp;
};

/* opaque handle */
struct pm_dev;

typedef void (*pm_callback_t)(struct pm_dev *dev, const struct pm_state *state,
        void *ctx);

/* --- factory functions --- */

struct pm_dev *pm_alloc_adc(const char *name, const char *adc_dev, float scale);
struct pm_dev *pm_alloc_digital(const char *name, const char *protocol,
                const char *dev_path, uint32_t addr);
struct pm_dev *pm_alloc_generic(const char *name, const char *charger_node,
                const char *capacity_node, void *args);

/* --- lifecycle --- */

int pm_init(struct pm_dev *dev, const struct pm_config *cfg);
void pm_set_callback(struct pm_dev *dev, pm_callback_t cb, void *ctx);
int pm_start(struct pm_dev *dev, uint32_t freq_hz);
int pm_get_state(struct pm_dev *dev, struct pm_state *out_state);
void pm_free(struct pm_dev *dev);

/* --- power switch control --- */

int pm_switch_set(struct pm_dev *dev, const char *channel_name, bool enable);
bool pm_switch_get(struct pm_dev *dev, const char *channel_name);

#ifdef __cplusplus
}
#endif

#endif /* PM_H */
