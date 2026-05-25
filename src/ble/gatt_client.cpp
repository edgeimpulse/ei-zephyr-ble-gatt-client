/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ble/gatt_client.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gatt_client, LOG_LEVEL_INF);

// Service and characteristic UUIDs
static struct bt_uuid_128 ei_service_uuid = BT_UUID_INIT_128(BT_UUID_EI_SERVICE_VAL);
static struct bt_uuid_128 inference_char_uuid = BT_UUID_INIT_128(BT_UUID_INFERENCE_CHAR_VAL);
static struct bt_uuid_128 sensor_char_uuid = BT_UUID_INIT_128(BT_UUID_SENSOR_CHAR_VAL);
static struct bt_uuid_128 state_char_uuid = BT_UUID_INIT_128(BT_UUID_STATE_CHAR_VAL);

// Connection and discovery state
static struct bt_conn *default_conn = NULL;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params inference_sub_params;
static struct bt_gatt_subscribe_params sensor_sub_params;

// Characteristic handles
static uint16_t inference_handle = 0;
static uint16_t sensor_handle = 0;
static uint16_t state_handle = 0;
static uint16_t inference_ccc_handle = 0;
static uint16_t sensor_ccc_handle = 0;

// Track which characteristic discovery is currently in flight so the
// descriptor-discovery callback can attribute the CCC handle correctly.
static enum {
    CHAR_NONE,
    CHAR_INFERENCE,
    CHAR_SENSOR,
} current_discover_char = CHAR_NONE;

// Callbacks
static inference_callback_t inference_cb = NULL;
static connection_callback_t connection_cb = NULL;
static sensor_data_callback_t sensor_cb = NULL;

// Scan parameters
static const char *target_device_name = "EI-Golioth";
static bool scanning = false;

/**
 * @brief Parse inference result from BLE notification
 */
static void parse_inference_result(const void *data, uint16_t length, inference_result_t *result)
{
    if (length < sizeof(inference_result_t)) {
        LOG_WRN("Inference data too short: %d bytes", length);
        return;
    }

    memcpy(result, data, sizeof(inference_result_t));
    result->timestamp = k_uptime_get();

    LOG_INF("Inference: %s (%.2f%%) DSP:%ums CLS:%ums",
            result->label,
            result->confidence * 100.0f,
            result->dsp_time_ms,
            result->classification_time_ms);
}

/**
 * @brief Notification callback
 */
static uint8_t notify_callback(struct bt_conn *conn,
                               struct bt_gatt_subscribe_params *params,
                               const void *data, uint16_t length)
{
    if (!data) {
        LOG_INF("Unsubscribed from notifications");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    if (params->value_handle == inference_handle) {
        inference_result_t result;
        parse_inference_result(data, length, &result);

        if (inference_cb) {
            inference_cb(&result);
        }
    } else if (params->value_handle == sensor_handle) {
        if (sensor_cb) {
            sensor_cb(static_cast<const float *>(data), length / sizeof(float));
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Subscribe to characteristic notifications
 */
static int subscribe_inference(struct bt_conn *conn)
{
    if (!inference_ccc_handle) {
        LOG_ERR("Inference CCC handle not discovered");
        return -EINVAL;
    }

    inference_sub_params.notify = notify_callback;
    inference_sub_params.value = BT_GATT_CCC_NOTIFY;
    inference_sub_params.value_handle = inference_handle;
    inference_sub_params.ccc_handle = inference_ccc_handle;
    inference_sub_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;

    int err = bt_gatt_subscribe(conn, &inference_sub_params);
    if (err < 0 && err != -EALREADY) {
        LOG_ERR("Subscribe (inference) failed: %d", err);
        return err;
    }

    LOG_INF("Subscribed to inference notifications");
    return 0;
}

/**
 * @brief Subscribe to sensor data notifications (raw IMU windows)
 */
static int subscribe_sensor(struct bt_conn *conn)
{
    if (!sensor_ccc_handle) {
        LOG_WRN("Sensor CCC handle not discovered \u2014 skipping sensor subscription");
        return -EINVAL;
    }

    sensor_sub_params.notify = notify_callback;
    sensor_sub_params.value = BT_GATT_CCC_NOTIFY;
    sensor_sub_params.value_handle = sensor_handle;
    sensor_sub_params.ccc_handle = sensor_ccc_handle;
    sensor_sub_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;

    int err = bt_gatt_subscribe(conn, &sensor_sub_params);
    if (err < 0 && err != -EALREADY) {
        LOG_ERR("Subscribe (sensor) failed: %d", err);
        return err;
    }

    LOG_INF("Subscribed to sensor data notifications");
    return 0;
}

/**
 * @brief GATT discovery callback
 */
static uint8_t discover_func(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            struct bt_gatt_discover_params *params)
{
    if (!attr) {
        LOG_INF("Discovery complete");
        memset(params, 0, sizeof(*params));
        current_discover_char = CHAR_NONE;

        // Subscribe to notifications. Sensor subscription is best-effort — some
        // peripherals don't expose a sensor characteristic and that is OK.
        if (inference_handle && inference_ccc_handle) {
            subscribe_inference(conn);
        }
        if (sensor_handle && sensor_ccc_handle) {
            subscribe_sensor(conn);
        }

        return BT_GATT_ITER_STOP;
    }

    LOG_INF("Discovered attribute: handle %u", attr->handle);

    // Check if this is our service
    if (params->type == BT_GATT_DISCOVER_PRIMARY &&
        !bt_uuid_cmp(params->uuid, &ei_service_uuid.uuid)) {
        LOG_INF("Found EI service");

        // Start discovering characteristics
        discover_params.uuid = NULL;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        bt_gatt_discover(conn, &discover_params);
        return BT_GATT_ITER_STOP;
    }

    // Check characteristics
    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;

        if (!bt_uuid_cmp(chrc->uuid, &inference_char_uuid.uuid)) {
            LOG_INF("Found inference characteristic");
            inference_handle = chrc->value_handle;

            // Discover CCC descriptor for the inference characteristic
            current_discover_char = CHAR_INFERENCE;
            discover_params.uuid = BT_UUID_GATT_CCC;
            discover_params.start_handle = chrc->value_handle + 1;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            bt_gatt_discover(conn, &discover_params);

            return BT_GATT_ITER_STOP;
        } else if (!bt_uuid_cmp(chrc->uuid, &sensor_char_uuid.uuid)) {
            LOG_INF("Found sensor characteristic");
            sensor_handle = chrc->value_handle;

            // Discover CCC descriptor for the sensor characteristic so we can
            // also subscribe to raw IMU notifications.
            current_discover_char = CHAR_SENSOR;
            discover_params.uuid = BT_UUID_GATT_CCC;
            discover_params.start_handle = chrc->value_handle + 1;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            bt_gatt_discover(conn, &discover_params);

            return BT_GATT_ITER_STOP;
        } else if (!bt_uuid_cmp(chrc->uuid, &state_char_uuid.uuid)) {
            LOG_INF("Found state characteristic");
            state_handle = chrc->value_handle;
        }
    }

    // Check for CCC descriptor
    if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
        if (current_discover_char == CHAR_INFERENCE) {
            inference_ccc_handle = attr->handle;
            LOG_INF("Found inference CCC descriptor: handle %u", inference_ccc_handle);
        } else if (current_discover_char == CHAR_SENSOR) {
            sensor_ccc_handle = attr->handle;
            LOG_INF("Found sensor CCC descriptor: handle %u", sensor_ccc_handle);
        }

        // Resume characteristic discovery to find the next char in the service.
        current_discover_char = CHAR_NONE;
        discover_params.uuid = NULL;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        bt_gatt_discover(conn, &discover_params);
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Start GATT service discovery
 */
static int start_discovery(struct bt_conn *conn)
{
    inference_handle = 0;
    sensor_handle = 0;
    state_handle = 0;
    inference_ccc_handle = 0;
    sensor_ccc_handle = 0;
    current_discover_char = CHAR_NONE;

    memset(&discover_params, 0, sizeof(discover_params));

    discover_params.uuid = &ei_service_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_ERR("Discovery failed: %d", err);
        return err;
    }

    LOG_INF("Started GATT discovery");
    return 0;
}

/**
 * @brief Connection callback
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed: %u", err);
        if (connection_cb) {
            connection_cb(false);
        }
        return;
    }

    default_conn = bt_conn_ref(conn);

    LOG_INF("Connected");

    if (connection_cb) {
        connection_cb(true);
    }

    // Start service discovery
    start_discovery(conn);
}

/**
 * @brief Disconnection callback
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected: reason %u", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }

    if (connection_cb) {
        connection_cb(false);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/**
 * @brief Scan callback
 */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi,
                   uint8_t type, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    char name[31];
    uint8_t name_len = 0;

    // Parse advertisement data for device name
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        if (ad_type == BT_DATA_NAME_COMPLETE || ad_type == BT_DATA_NAME_SHORTENED) {
            name_len = len - 1;
            if (name_len > sizeof(name) - 1) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, ad->data, name_len);
            name[name_len] = '\0';
            break;
        }

        net_buf_simple_pull(ad, len - 1);
    }

    if (name_len > 0 && strcmp(name, target_device_name) == 0) {
        bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
        LOG_INF("Found %s (RSSI: %d dBm)", name, rssi);

        // Stop scanning and connect
        bt_le_scan_stop();
        scanning = false;

        int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                                    BT_LE_CONN_PARAM_DEFAULT,
                                    &default_conn);
        if (err) {
            LOG_ERR("Create connection failed: %d", err);
        }
    }
}

/**
 * @brief Initialize BLE GATT client
 */
int gatt_client_init(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed: %d", err);
        return err;
    }

    LOG_INF("Bluetooth initialized");
    return 0;
}

/**
 * @brief Start scanning for devices
 */
int gatt_client_start_scan(void)
{
    int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, scan_cb);
    if (err) {
        LOG_ERR("Scan start failed: %d", err);
        return err;
    }

    scanning = true;
    LOG_INF("Scanning for %s...", target_device_name);
    return 0;
}

/**
 * @brief Stop scanning
 */
int gatt_client_stop_scan(void)
{
    int err = bt_le_scan_stop();
    if (err) {
        LOG_ERR("Scan stop failed: %d", err);
        return err;
    }

    scanning = false;
    LOG_INF("Scanning stopped");
    return 0;
}

/**
 * @brief Connect to device by name
 */
int gatt_client_connect(const char *device_name)
{
    target_device_name = device_name;
    return gatt_client_start_scan();
}

/**
 * @brief Disconnect from device
 */
int gatt_client_disconnect(void)
{
    if (!default_conn) {
        return -ENOTCONN;
    }

    int err = bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) {
        LOG_ERR("Disconnect failed: %d", err);
        return err;
    }

    return 0;
}

/**
 * @brief Check if connected
 */
bool gatt_client_is_connected(void)
{
    return default_conn != NULL;
}

/**
 * @brief Register callbacks
 */
void gatt_client_register_inference_callback(inference_callback_t cb)
{
    inference_cb = cb;
}

void gatt_client_register_connection_callback(connection_callback_t cb)
{
    connection_cb = cb;
}

void gatt_client_register_sensor_callback(sensor_data_callback_t cb)
{
    sensor_cb = cb;
}

/**
 * @brief Enable notifications
 */
int gatt_client_enable_inference_notifications(void)
{
    if (!default_conn) {
        return -ENOTCONN;
    }

    return subscribe_inference(default_conn);
}

/**
 * @brief Read device state
 */
int gatt_client_read_device_state(char *state, size_t len)
{
    if (!default_conn || !state_handle) {
        return -EINVAL;
    }

    // Implement GATT read operation
    LOG_INF("Reading device state...");
    return 0;
}
