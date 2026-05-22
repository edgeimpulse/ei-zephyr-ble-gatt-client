/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EI_SENSOR_H
#define EI_SENSOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Number of float values in one raw IMU sample (accel XYZ + gyro XYZ).
 * This matches a typical 6-axis Edge Impulse impulse.
 */
#define EI_SENSOR_IMU_AXES 6

/**
 * @brief Initialise all available on-board sensors.
 *
 * Must be called before ei_sensor_collect() or ei_sensor_run_loop().
 * BLE must already be enabled (gatt_client_init() or bt_enable()).
 *
 * @return 0 on success, negative errno on failure.
 */
int ei_sensor_init(void);

/**
 * @brief Collect one IMU sample.
 *
 * Fills @p out_buf with [accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z]
 * in SI units (m/s² and rad/s).
 *
 * @param out_buf  Output buffer (must hold at least EI_SENSOR_IMU_AXES floats).
 * @param len      Capacity of @p out_buf.
 * @return Number of values written (EI_SENSOR_IMU_AXES), or negative errno.
 */
int ei_sensor_collect(float *out_buf, size_t len);

/**
 * @brief Blocking sensor loop: sample → infer (if model present) → BLE notify.
 *
 * Call this instead of gatt_client_start_scan() when CONFIG_EI_SENSOR_LOCAL=y.
 * Does not return.
 */
void ei_sensor_run_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* EI_SENSOR_H */
