/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "gatt_server.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(gatt_server, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Shared service / characteristic UUIDs  (match gatt_client.h definitions)
 * -------------------------------------------------------------------------- */
static struct bt_uuid_128 ei_svc_uuid =
    BT_UUID_INIT_128(BT_UUID_EI_SERVICE_VAL);
static struct bt_uuid_128 inf_chr_uuid =
    BT_UUID_INIT_128(BT_UUID_INFERENCE_CHAR_VAL);
static struct bt_uuid_128 sensor_chr_uuid =
    BT_UUID_INIT_128(BT_UUID_SENSOR_CHAR_VAL);

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */
static inference_result_t last_inference;
static float              last_sensor_buf[64];
static size_t             last_sensor_len = 0;

static bool inference_notify_enabled = false;
static bool sensor_notify_enabled    = false;

/* Connection to the Android central (NULL when nobody is connected) */
static struct bt_conn *android_conn = NULL;

/* --------------------------------------------------------------------------
 * GATT attribute read callbacks
 * -------------------------------------------------------------------------- */
static ssize_t inference_read_cb(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &last_inference, sizeof(last_inference));
}

static ssize_t sensor_read_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             last_sensor_buf,
                             last_sensor_len * sizeof(float));
}

/* --------------------------------------------------------------------------
 * CCC (Client Characteristic Configuration) changed callbacks
 * -------------------------------------------------------------------------- */
static void inf_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    inference_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Inference notifications %s",
            inference_notify_enabled ? "enabled" : "disabled");
}

static void sensor_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    sensor_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Sensor notifications %s",
            sensor_notify_enabled ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------
 * GATT service definition (compile-time; auto-registered at BT init)
 * -------------------------------------------------------------------------- */
BT_GATT_SERVICE_DEFINE(ei_server_svc,
    BT_GATT_PRIMARY_SERVICE(&ei_svc_uuid),

    /* Inference result characteristic */
    BT_GATT_CHARACTERISTIC(&inf_chr_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           inference_read_cb, NULL, &last_inference),
    BT_GATT_CCC(inf_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Raw sensor data characteristic */
    BT_GATT_CHARACTERISTIC(&sensor_chr_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           sensor_read_cb, NULL, NULL),
    BT_GATT_CCC(sensor_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* --------------------------------------------------------------------------
 * Connection tracking for Android clients
 * -------------------------------------------------------------------------- */
static void android_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        return;
    }

    /* Only track connections that arrived on our advertisement (peripheral) */
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) == 0 &&
        info.role == BT_CONN_ROLE_PERIPHERAL) {
        android_conn = bt_conn_ref(conn);
        LOG_INF("Android central connected");
    }
}

static void android_disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (android_conn == conn) {
        bt_conn_unref(android_conn);
        android_conn = NULL;
        LOG_INF("Android central disconnected (reason %u)", reason);

        /* Restart advertising so Android can reconnect */
        int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, NULL, 0, NULL, 0);
        if (err && err != -EALREADY) {
            LOG_ERR("Adv restart failed: %d", err);
        }
    }
}

/* Register connection callbacks alongside the existing ones from gatt_client.cpp */
BT_CONN_CB_DEFINE(server_conn_callbacks) = {
    .connected    = android_connected,
    .disconnected = android_disconnected,
};

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

int gatt_server_init(void)
{
    /*
     * Advertising payload:
     *   AD:  Flags + full service UUID (128-bit)
     *   SD:  Device name (set by CONFIG_BT_DEVICE_NAME="EI-Monitor")
     */
    static const uint8_t svc_uuid_le[] = {
        /* BT_UUID_EI_SERVICE_VAL in little-endian wire order */
        0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
        0x34, 0x12, 0x78, 0x56, 0x78, 0x56, 0x34, 0x12
    };

    static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS,
                      BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_UUID128_ALL,
                svc_uuid_le, sizeof(svc_uuid_le)),
    };

    int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME,
                              ad, ARRAY_SIZE(ad),
                              NULL, 0);
    if (err) {
        LOG_ERR("Advertising start failed: %d", err);
        return err;
    }

    LOG_INF("EI GATT server advertising as \"%s\"",
            CONFIG_BT_DEVICE_NAME);
    return 0;
}

void gatt_server_notify_inference(const inference_result_t *result)
{
    if (!result || !android_conn || !inference_notify_enabled) {
        return;
    }

    memcpy(&last_inference, result, sizeof(last_inference));

    /* Find the inference characteristic value attribute handle */
    const struct bt_gatt_attr *attr =
        bt_gatt_find_by_uuid(ei_server_svc.attrs,
                             ei_server_svc.attr_count,
                             &inf_chr_uuid.uuid);
    if (!attr) {
        LOG_WRN("Inference attr not found");
        return;
    }

    int err = bt_gatt_notify(android_conn, attr,
                             &last_inference, sizeof(last_inference));
    if (err) {
        LOG_WRN("Inference notify failed: %d", err);
    }
}

void gatt_server_notify_sensor_data(const float *data, size_t len)
{
    if (!data || !android_conn || !sensor_notify_enabled || len == 0) {
        return;
    }

    if (len > ARRAY_SIZE(last_sensor_buf)) {
        len = ARRAY_SIZE(last_sensor_buf);
    }
    memcpy(last_sensor_buf, data, len * sizeof(float));
    last_sensor_len = len;

    const struct bt_gatt_attr *attr =
        bt_gatt_find_by_uuid(ei_server_svc.attrs,
                             ei_server_svc.attr_count,
                             &sensor_chr_uuid.uuid);
    if (!attr) {
        return;
    }

    int err = bt_gatt_notify(android_conn, attr,
                             last_sensor_buf, len * sizeof(float));
    if (err) {
        LOG_WRN("Sensor notify failed: %d", err);
    }
}
