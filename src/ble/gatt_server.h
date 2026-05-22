/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GATT_SERVER_H
#define GATT_SERVER_H

#include "gatt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start advertising the EI GATT service so an Android phone can
 *        connect as a central and subscribe to inference / sensor notifications.
 *
 * Must be called after gatt_client_init() (which calls bt_enable).
 *
 * @return 0 on success, negative error code otherwise.
 */
int gatt_server_init(void);

/**
 * @brief Notify all subscribed centrals (Android) of a new inference result.
 *
 * @param result Pointer to the inference result to send.
 */
void gatt_server_notify_inference(const inference_result_t *result);

/**
 * @brief Notify all subscribed centrals (Android) of raw sensor samples.
 *
 * @param data  Pointer to float array.
 * @param len   Number of floats.
 */
void gatt_server_notify_sensor_data(const float *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVER_H */
