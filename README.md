# Edge Impulse BLE GATT Client

A Zephyr application that bridges Edge Impulse inference results to an Android app over BLE. It supports two operating modes selected automatically at build time:

| Mode | Board | How it works |
|------|-------|-------------|
| **Relay** (default) | Nordic Thingy:53 | BLE central — connects to an `EI-Golioth` peripheral, subscribes to inference + sensor notifications, re-advertises them to Android via a GATT server |
| **Local sensor** | Arduino Nano 33 BLE Sense | Reads on-board IMU (and all other sensors) directly, runs an optional EI impulse locally, streams raw data + inference results to Android via BLE GATT server |

**Compatible with:** [example-standalone-inferencing-zephyr-module](https://github.com/edgeimpulse/example-standalone-inferencing-zephyr-module) — drop a Zephyr model export next to the project and local inference is enabled automatically.

---

## Supported boards

| Board | Zephyr ID | Mode |
|-------|-----------|------|
| Nordic Thingy:53 | `thingy53/nrf5340/cpuapp` | Relay |
| Arduino Nano 33 BLE Sense | `arduino_nano_33_ble` | Local sensor |

Any board with an on-board IMU declared in the Zephyr devicetree can be used in local-sensor mode — add `boards/<board>.conf` with `CONFIG_EI_SENSOR_LOCAL=y` and a matching board overlay.

---

## Sensors (Arduino Nano 33 BLE Sense)

All sensors are read in local-sensor mode and streamed as a packed float array over BLE:

| Index | Sensor | Channel | Unit |
|-------|--------|---------|------|
| 0–2 | LSM9DS1 accel | X, Y, Z | m/s² |
| 3–5 | LSM9DS1 gyro | X, Y, Z | rad/s |
| 6–8 | LSM9DS1 mag | X, Y, Z | Gauss |
| 9 | HTS221 | Temperature | °C |
| 10 | HTS221 | Humidity | % RH |
| 11 | LPS22HB | Pressure | kPa |
| 12 | APDS9960 | Proximity | — |
| 13–15 | APDS9960 | R, G, B | — |

---

## Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) ≥ 0.16
- `west` ≥ 1.2
- CMake ≥ 3.20, Python ≥ 3.8

---

## Getting started

### 1. Initialise the workspace

```bash
west init -m https://github.com/edgeimpulse/ei-zephyr-ble-gatt-client.git --mr main myproject
cd myproject
west update
```

This fetches Zephyr, the Edge Impulse SDK Zephyr module, and all dependencies.

### 2. (Optional) Add an EI impulse for local inference

Export your trained impulse from [Edge Impulse Studio](https://studio.edgeimpulse.com):
**Deployment → Zephyr library → Build** → download and unzip as `model/` alongside `ei-zephyr-ble-gatt-client/`.

```
myproject/
  ei-zephyr-ble-gatt-client/   ← this repo
  model/                        ← EI Zephyr library export
    CMakeLists.txt
    edge-impulse-sdk/
    tflite-model/
    ...
  zephyr/
  modules/
```

If `model/` is absent the project builds without inference (sensor streaming only).

### 3. Build

**Thingy:53 — relay mode**
```bash
west build --pristine -b thingy53/nrf5340/cpuapp ei-zephyr-ble-gatt-client
```

**Arduino Nano 33 BLE Sense — local sensor mode**
```bash
west build --pristine -b arduino_nano_33_ble ei-zephyr-ble-gatt-client
```

### 4. Flash

```bash
west flash
```

### 5. Open a serial console

```bash
# macOS — find port
ls /dev/tty.usbmodem*

screen /dev/tty.usbmodemXXXX 115200
# or
minicom -D /dev/tty.usbmodemXXXX -b 115200
```

---

## BLE GATT service

Both modes expose the same GATT service so the same Android app works with either board.

| Service UUID | `12345678-1234-5678-1234-56789abcdef0` |
|---|---|
| Inference char | `...def1` — READ + NOTIFY — `inference_result_t` |
| Sensor data char | `...def2` — READ + NOTIFY — packed `float[]` |
| Device state char | `...def3` — READ + WRITE — status string |

```cpp
// inference_result_t layout (gatt_client.h)
struct inference_result_t {
    char     label[32];
    float    confidence;          // 0.0–1.0
    uint32_t dsp_time_ms;
    uint32_t classification_time_ms;
    uint64_t timestamp;
};
```

---

## Project structure

```
ei-zephyr-ble-gatt-client/
├── CMakeLists.txt           # Adds optional model/ module before find_package(Zephyr)
├── Kconfig                  # EI_SENSOR_LOCAL + EI_SENSOR_SAMPLE_INTERVAL_MS
├── prj.conf                 # BLE central + peripheral, C++17, logging
├── west.yml                 # Zephyr v4.0.0 + edge-impulse-sdk-zephyr
├── boards/
│   ├── thingy53_nrf5340_cpuapp.overlay  # UART config (relay mode)
│   ├── arduino_nano_33_ble.overlay      # Enable all on-board sensors
│   └── arduino_nano_33_ble.conf         # Sensor drivers + EI_SENSOR_LOCAL=y
└── src/
    ├── main.cpp             # Branches on CONFIG_EI_SENSOR_LOCAL at compile time
    ├── ble/
    │   ├── gatt_client.h/.cpp   # BLE central: scan, connect, subscribe
    │   └── gatt_server.h/.cpp   # BLE peripheral: advertise, notify Android
    └── sensors/
        ├── ei_sensor.h          # Sensor API
        └── ei_sensor.cpp        # LSM9DS1, HTS221, LPS22HB, APDS9960 + EI inference
```

---

## Kconfig options

| Symbol | Default | Description |
|--------|---------|-------------|
| `CONFIG_EI_SENSOR_LOCAL` | `n` (y via `arduino_nano_33_ble.conf`) | Read sensors locally; skip BLE scan |
| `CONFIG_EI_SENSOR_SAMPLE_INTERVAL_MS` | `10` | Sampling period in ms (100 Hz default) |

---

## Adding a new board

1. Create `boards/<board>.overlay` — enable the IMU node in devicetree.
2. Create `boards/<board>.conf`:
   ```
   CONFIG_I2C=y          # or SPI
   CONFIG_SENSOR=y
   CONFIG_<IMU_DRIVER>=y
   CONFIG_EI_SENSOR_LOCAL=y
   ```
3. If your IMU is not LSM9DS1, BMI270, or FXOS8700, add a `DT_HAS_COMPAT_STATUS_OKAY` branch in `src/sensors/ei_sensor.cpp`.
