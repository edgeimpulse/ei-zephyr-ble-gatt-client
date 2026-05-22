/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble/gatt_client.h"
#include "ble/gatt_server.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// Connection state
static bool is_connected = false;

/**
 * @brief Handle inference result from BLE peripheral
 */
static void on_inference_received(const inference_result_t *result)
{
    printk("\n=== Inference Result ===\n");
    printk("Label: %s\n", result->label);
    printk("Confidence: %.1f%%\n", result->confidence * 100.0f);
    printk("DSP Time: %u ms\n", result->dsp_time_ms);
    printk("Classification Time: %u ms\n", result->classification_time_ms);
    printk("Total Time: %u ms\n", result->dsp_time_ms + result->classification_time_ms);
    printk("========================\n\n");

    /* Relay result to any connected Android central */
    gatt_server_notify_inference(result);
}

/**
 * @brief Handle connection state changes
 */
static void on_connection_changed(bool connected)
{
    is_connected = connected;

    if (connected) {
        printk("\n>>> Connected to EI-Golioth device!\n");
        
        // Small delay then enable notifications
        k_sleep(K_MSEC(500));
        gatt_client_enable_inference_notifications();
    } else {
        printk("\n>>> Disconnected from device\n");
        printk(">>> Rescanning in 2 seconds...\n\n");
        
        // Restart scanning
        k_sleep(K_SECONDS(2));
        gatt_client_start_scan();
    }
}

/**
 * @brief Handle sensor data from BLE peripheral
 */
static void on_sensor_data_received(const float *data, size_t len)
{
    LOG_DBG("Sensor data received: %zu samples", len);
    /* Relay raw sensor samples to any connected Android central */
    gatt_server_notify_sensor_data(data, len);
}

int main(void)
{
    printk("\n");
    printk("========================================\n");
    printk("  Edge Impulse BLE GATT Client\n");
    printk("  Monitoring EI-Golioth IMU Devices\n");
    printk("========================================\n\n");

    // Initialize BLE GATT client
    int err = gatt_client_init();
    if (err) {
        printk("ERROR: GATT client init failed: %d\n", err);
        return err;
    }

    // Register callbacks
    gatt_client_register_inference_callback(on_inference_received);
    gatt_client_register_connection_callback(on_connection_changed);
    gatt_client_register_sensor_callback(on_sensor_data_received);

    // Start the GATT server so Android can connect and receive relayed data
    err = gatt_server_init();
    if (err) {
        printk("WARNING: GATT server init failed: %d (continuing)\n", err);
    }

    // Start scanning for EI-Golioth devices
    printk("Scanning for EI-Golioth devices...\n\n");
    err = gatt_client_start_scan();
    if (err) {
        printk("ERROR: Failed to start scan: %d\n", err);
        return err;
    }

    // Main loop - just keep alive
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
