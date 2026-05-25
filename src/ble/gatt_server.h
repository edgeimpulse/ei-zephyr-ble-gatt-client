/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GATT_SERVER_H
#define GATT_SERVER_H

#include <stdbool.h>

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

/**
 * @brief Current capture label (null-terminated, max 16 bytes).
 *
 * Updated by Android via writes to the STATE characteristic. Defaults to
 * "idle". Use this to gate / tag local data collection.
 */
const char *gatt_server_get_label(void);

/** Callback invoked whenever the Android central writes a new label. */
typedef void (*label_changed_cb_t)(const char *label);
void gatt_server_register_label_callback(label_changed_cb_t cb);

/**
 * @brief Whether an Android central is currently connected.
 *
 * Used by the local sensor loop to avoid running the I2C/SPI sample fetch
 * at 100 Hz when nobody is listening, which keeps the radio free for
 * advertising and avoids power spikes that can brown-out the board.
 */
bool gatt_server_is_central_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVER_H */
