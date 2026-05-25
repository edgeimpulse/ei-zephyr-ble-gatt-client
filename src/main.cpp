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
#include "sensors/ei_sensor.h"

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

#ifdef CONFIG_EI_SENSOR_LOCAL
    printk("  Mode: Local Sensor Collection\n");
    printk("  Board: %s\n", CONFIG_BOARD);
#else
    printk("  Mode: BLE Relay (EI-Golioth Monitor)\n");
#endif

    printk("========================================\n\n");

    /*
     * gatt_client_init() calls bt_enable() which is needed in both modes.
     * In local-sensor mode we skip start_scan() and drive the sensor loop
     * instead.
     */
    int err = gatt_client_init();
    if (err) {
        printk("ERROR: BLE init failed: %d\n", err);
        return err;
    }

    /* Start the GATT server so Android can connect and receive data */
    err = gatt_server_init();
    if (err) {
        printk("WARNING: GATT server init failed: %d (continuing)\n", err);
    }

#ifdef CONFIG_EI_SENSOR_LOCAL
    /*
     * Local sensor mode — Arduino Nano 33 BLE Sense (or any board with
     * an on-board IMU declared in the devicetree).
     *
     * Sensors sampled at CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS (default 10 ms
     * = 100 Hz). Raw data is streamed to Android; if an EI model is present
     * at build time inference results are also notified.
     */
    err = ei_sensor_init();
    if (err) {
        printk("ERROR: Sensor init failed: %d\n", err);
        return err;
    }

    printk("Starting local sensor collection (sampling every %d ms)...\n\n",
           CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS);

    /* Does not return */
    ei_sensor_run_loop();

#else
    /*
     * Relay mode — Thingy:53 (or any board without on-board IMU).
     *
     * Scans for an "EI-Golioth" BLE peripheral, subscribes to inference
     * and sensor notifications, then re-advertises them to an Android central
     * via the GATT server.
     */
    gatt_client_register_inference_callback(on_inference_received);
    gatt_client_register_connection_callback(on_connection_changed);
    gatt_client_register_sensor_callback(on_sensor_data_received);

    printk("Scanning for EI-Golioth devices...\n\n");
    err = gatt_client_start_scan();
    if (err) {
        printk("ERROR: Failed to start scan: %d\n", err);
        return err;
    }

    while (1) {
        k_sleep(K_SECONDS(1));
    }
#endif

    return 0;
}
