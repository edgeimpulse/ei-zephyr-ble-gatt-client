/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Local sensor collection for Edge Impulse BLE GATT Client.
 *
 * Supports the following on-board sensors (compiled in based on devicetree):
 *   st,lsm9ds1       — accel + gyro   (Arduino Nano 33 BLE Sense)
 *   st,lsm9ds1-magn  — magnetometer
 *   st,hts221        — temp + humidity
 *   st,lps22hb-press — pressure
 *   avago,apds9960   — proximity / colour
 *
 * To add a different IMU, add a DT_HAS_COMPAT_STATUS_OKAY() branch and
 * derive accel/gyro values from the appropriate SENSOR_CHAN_* reads.
 */

#include "ei_sensor.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "ble/gatt_server.h"

LOG_MODULE_REGISTER(ei_sensor, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Device bindings — resolved at compile time from devicetree.
 * -------------------------------------------------------------------------- */

/* Primary IMU: accel + gyro */
#if DT_HAS_COMPAT_STATUS_OKAY(st_lsm9ds1)
static const struct device *const imu_dev = DEVICE_DT_GET_ANY(st_lsm9ds1);
#define IMU_LABEL "LSM9DS1"
#elif DT_HAS_COMPAT_STATUS_OKAY(bosch_bmi270)
static const struct device *const imu_dev = DEVICE_DT_GET_ANY(bosch_bmi270);
#define IMU_LABEL "BMI270"
#elif DT_HAS_COMPAT_STATUS_OKAY(nxp_fxos8700)
static const struct device *const imu_dev = DEVICE_DT_GET_ANY(nxp_fxos8700);
#define IMU_LABEL "FXOS8700"
#else
#error "No supported IMU found in devicetree. Add a boards/<board>.overlay."
#endif

/* Magnetometer (optional) */
#if DT_HAS_COMPAT_STATUS_OKAY(st_lsm9ds1_magn)
static const struct device *const mag_dev = DEVICE_DT_GET_ANY(st_lsm9ds1_magn);
#define HAS_MAG 1
#endif

/* Temperature + Humidity (optional) */
#if DT_HAS_COMPAT_STATUS_OKAY(st_hts221)
static const struct device *const hts_dev = DEVICE_DT_GET_ANY(st_hts221);
#define HAS_HTS221 1
#endif

/* Pressure (optional) */
#if DT_HAS_COMPAT_STATUS_OKAY(st_lps22hb_press)
static const struct device *const press_dev = DEVICE_DT_GET_ANY(st_lps22hb_press);
#define HAS_LPS22HB 1
#endif

/* Proximity + Colour (optional) */
#if DT_HAS_COMPAT_STATUS_OKAY(avago_apds9960)
static const struct device *const apds_dev = DEVICE_DT_GET_ANY(avago_apds9960);
#define HAS_APDS9960 1
#endif

/* --------------------------------------------------------------------------
 * Helper: convert struct sensor_value to float
 * -------------------------------------------------------------------------- */
static inline float sv2f(const struct sensor_value *v)
{
	return (float)v->val1 + (float)v->val2 / 1000000.0f;
}

/* --------------------------------------------------------------------------
 * Optional EI inference — compiled in only when a model is present.
 * Export your impulse from EI Studio as a Zephyr library, drop the
 * extracted model/ folder next to this project, then rebuild with west.
 * -------------------------------------------------------------------------- */
#ifdef CONFIG_EDGE_IMPULSE
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"

static float feature_buf[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static size_t feature_ix = 0;

static void run_inference(void)
{
	signal_t signal;
	ei_impulse_result_t result = {0};

	int r = numpy::signal_from_buffer(feature_buf,
					  EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
					  &signal);
	if (r != 0) {
		LOG_ERR("signal_from_buffer failed: %d", r);
		return;
	}

	r = run_classifier(&signal, &result, false);
	if (r != EI_IMPULSE_OK) {
		LOG_ERR("run_classifier failed: %d", r);
		return;
	}

	/* Find the best classification */
	size_t best = 0;
	for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
		if (result.classification[i].value > result.classification[best].value) {
			best = i;
		}
	}

	inference_result_t inf = {
		.confidence = result.classification[best].value,
		.dsp_time_ms = (uint32_t)result.timing.dsp,
		.classification_time_ms = (uint32_t)result.timing.classification,
		.timestamp = (uint64_t)k_uptime_get(),
	};
	strncpy(inf.label, result.classification[best].label, sizeof(inf.label) - 1);
	inf.label[sizeof(inf.label) - 1] = '\0';

	LOG_INF("Inference: %s (%.1f%%)  DSP %u ms  CLS %u ms",
		inf.label, inf.confidence * 100.0f,
		inf.dsp_time_ms, inf.classification_time_ms);

	gatt_server_notify_inference(&inf);
}
#endif /* CONFIG_EDGE_IMPULSE */

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

int ei_sensor_init(void)
{
	if (!device_is_ready(imu_dev)) {
		LOG_ERR("%s device not ready", IMU_LABEL);
		return -ENODEV;
	}
	LOG_INF("%s initialised", IMU_LABEL);

#ifdef HAS_MAG
	if (!device_is_ready(mag_dev)) {
		LOG_WRN("Magnetometer not ready — skipping");
	} else {
		LOG_INF("Magnetometer initialised");
	}
#endif

#ifdef HAS_HTS221
	if (!device_is_ready(hts_dev)) {
		LOG_WRN("HTS221 not ready — skipping");
	} else {
		LOG_INF("HTS221 (temp/humidity) initialised");
	}
#endif

#ifdef HAS_LPS22HB
	if (!device_is_ready(press_dev)) {
		LOG_WRN("LPS22HB not ready — skipping");
	} else {
		LOG_INF("LPS22HB (pressure) initialised");
	}
#endif

#ifdef HAS_APDS9960
	if (!device_is_ready(apds_dev)) {
		LOG_WRN("APDS9960 not ready — skipping");
	} else {
		LOG_INF("APDS9960 (proximity/colour) initialised");
	}
#endif

	return 0;
}

int ei_sensor_collect(float *out_buf, size_t len)
{
	if (!out_buf || len < EI_SENSOR_IMU_AXES) {
		return -EINVAL;
	}

	struct sensor_value accel[3] = {}, gyro[3] = {};

	int err = sensor_sample_fetch(imu_dev);
	if (err) {
		LOG_WRN("IMU fetch error: %d", err);
		return err;
	}

	sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
	sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);

	out_buf[0] = sv2f(&accel[0]);
	out_buf[1] = sv2f(&accel[1]);
	out_buf[2] = sv2f(&accel[2]);
	out_buf[3] = sv2f(&gyro[0]);
	out_buf[4] = sv2f(&gyro[1]);
	out_buf[5] = sv2f(&gyro[2]);

	return EI_SENSOR_IMU_AXES;
}

void ei_sensor_run_loop(void)
{
	/*
	 * Full sensor payload streamed via BLE GATT:
	 *   [0..5]  accel XYZ (m/s²) + gyro XYZ (rad/s)  — always present
	 *   [6..8]  mag XYZ (Gauss)                        — if HAS_MAG
	 *   [9]     temperature (°C)                       — if HAS_HTS221
	 *   [10]    humidity (% RH)                        — if HAS_HTS221
	 *   [11]    pressure (kPa)                         — if HAS_LPS22HB
	 *   [12]    proximity                              — if HAS_APDS9960
	 *   [13..15] red, green, blue                      — if HAS_APDS9960
	 */
#define SENSOR_BUF_LEN 16
	float buf[SENSOR_BUF_LEN];
	int n;

	LOG_INF("Local sensor loop started (%d ms interval)",
		CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS);

	bool was_connected = false;
	while (1) {
		/* Idle until a central connects: don't hammer I2C at 100 Hz,
		 * keep the radio free for advertising, and avoid power spikes
		 * that can brown-out the board on USB power. */
		if (!gatt_server_is_central_connected()) {
			if (was_connected) {
				LOG_INF("Central disconnected — pausing sensor loop");
				was_connected = false;
			}
			k_sleep(K_MSEC(200));
			continue;
		}
		if (!was_connected) {
			LOG_INF("Central connected — resuming sensor loop");
			was_connected = true;
		}

		n = ei_sensor_collect(buf, SENSOR_BUF_LEN);
		if (n < 0) {
			LOG_WRN("IMU read failed: %d — retrying", n);
			k_sleep(K_MSEC(CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS));
			continue;
		}

		/* ---- Optional sensors ---- */
#ifdef HAS_MAG
		if (device_is_ready(mag_dev)) {
			struct sensor_value mag[3] = {};
			sensor_sample_fetch(mag_dev);
			sensor_channel_get(mag_dev, SENSOR_CHAN_MAGN_XYZ, mag);
			buf[n++] = sv2f(&mag[0]);
			buf[n++] = sv2f(&mag[1]);
			buf[n++] = sv2f(&mag[2]);
		}
#endif

#ifdef HAS_HTS221
		if (device_is_ready(hts_dev)) {
			struct sensor_value temp = {}, hum = {};
			sensor_sample_fetch(hts_dev);
			sensor_channel_get(hts_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(hts_dev, SENSOR_CHAN_HUMIDITY, &hum);
			buf[n++] = sv2f(&temp);
			buf[n++] = sv2f(&hum);
		}
#endif

#ifdef HAS_LPS22HB
		if (device_is_ready(press_dev)) {
			struct sensor_value press = {};
			sensor_sample_fetch(press_dev);
			sensor_channel_get(press_dev, SENSOR_CHAN_PRESS, &press);
			buf[n++] = sv2f(&press);
		}
#endif

#ifdef HAS_APDS9960
		if (device_is_ready(apds_dev)) {
			struct sensor_value prox = {}, r = {}, g = {}, b = {};
			sensor_sample_fetch(apds_dev);
			sensor_channel_get(apds_dev, SENSOR_CHAN_PROX, &prox);
			sensor_channel_get(apds_dev, SENSOR_CHAN_RED, &r);
			sensor_channel_get(apds_dev, SENSOR_CHAN_GREEN, &g);
			sensor_channel_get(apds_dev, SENSOR_CHAN_BLUE, &b);
			buf[n++] = sv2f(&prox);
			buf[n++] = sv2f(&r);
			buf[n++] = sv2f(&g);
			buf[n++] = sv2f(&b);
		}
#endif

		/* Stream raw sample to any connected Android central */
		gatt_server_notify_sensor_data(buf, (size_t)n);

#ifdef CONFIG_EDGE_IMPULSE
		/* Accumulate IMU features for EI inference */
		size_t copy = MIN((size_t)EI_SENSOR_IMU_AXES,
				  EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - feature_ix);
		memcpy(&feature_buf[feature_ix], buf, copy * sizeof(float));
		feature_ix += copy;

		if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
			run_inference();
			feature_ix = 0;
		}
#endif

		k_sleep(K_MSEC(CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS));
	}
}
