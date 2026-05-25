/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GATT_CLIENT_H
#define GATT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Edge Impulse + Golioth service UUIDs (match server)
#define BT_UUID_EI_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define BT_UUID_INFERENCE_CHAR_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define BT_UUID_SENSOR_CHAR_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

#define BT_UUID_STATE_CHAR_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)

// Inference result structure
// NOTE: packed so the on-wire layout matches Android's ZephyrBLEClient parser
//       (52 bytes, timestamp at offset 44). Without __packed__ the compiler
//       inserts 4 bytes of padding before `timestamp` on both ARM AAPCS and
//       RISC-V (uint64_t is 8-byte aligned), giving sizeof == 56.
typedef struct __attribute__((packed)) {
    char label[32];
    float confidence;
    uint32_t dsp_time_ms;
    uint32_t classification_time_ms;
    uint64_t timestamp;
} inference_result_t;

// Callback types
typedef void (*inference_callback_t)(const inference_result_t *result);
typedef void (*connection_callback_t)(bool connected);
typedef void (*sensor_data_callback_t)(const float *data, size_t len);

/**
 * @brief Initialize BLE GATT client
 * @return 0 on success
 */
int gatt_client_init(void);

/**
 * @brief Start scanning for EI-Golioth devices
 * @return 0 on success
 */
int gatt_client_start_scan(void);

/**
 * @brief Stop scanning
 * @return 0 on success
 */
int gatt_client_stop_scan(void);

/**
 * @brief Connect to discovered device
 * @param device_name Name of device to connect to
 * @return 0 on success
 */
int gatt_client_connect(const char *device_name);

/**
 * @brief Disconnect from device
 * @return 0 on success
 */
int gatt_client_disconnect(void);

/**
 * @brief Check if connected
 * @return true if connected
 */
bool gatt_client_is_connected(void);

/**
 * @brief Register inference result callback
 * @param cb Callback function
 */
void gatt_client_register_inference_callback(inference_callback_t cb);

/**
 * @brief Register connection state callback
 * @param cb Callback function
 */
void gatt_client_register_connection_callback(connection_callback_t cb);

/**
 * @brief Register sensor data callback
 * @param cb Callback function
 */
void gatt_client_register_sensor_callback(sensor_data_callback_t cb);

/**
 * @brief Enable notifications for inference results
 * @return 0 on success
 */
int gatt_client_enable_inference_notifications(void);

/**
 * @brief Read current device state
 * @param state Buffer to store state string
 * @param len Buffer length
 * @return 0 on success
 */
int gatt_client_read_device_state(char *state, size_t len);

#ifdef __cplusplus
}
#endif

#endif // GATT_CLIENT_H
