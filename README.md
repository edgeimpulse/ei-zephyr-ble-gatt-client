# Edge Impulse BLE GATT Client

A Zephyr application that bridges Edge Impulse inference results to an Android app over BLE. It supports two operating modes selected automatically at build time:

| Mode | Board | How it works |
|------|-------|-------------|
| **Relay** (default) | Nordic Thingy:53 | BLE central — connects to an `EI-Golioth` peripheral, subscribes to inference + sensor notifications, re-advertises them to Android via a GATT server |
| **Local sensor** | Arduino Nano 33 BLE Sense | Reads on-board LSM9DS1 IMU + HTS221, LPS22HB, APDS9960, runs an optional EI impulse locally, streams raw data + inference results to Android via BLE GATT server |
| **Local sensor** | Arduino Nesso N1 | Reads on-board BMI270 IMU (ESP32-C6), runs an optional EI impulse locally, streams raw data + inference results to Android via BLE GATT server |

**Compatible with:** [example-standalone-inferencing-zephyr-module](https://github.com/edgeimpulse/example-standalone-inferencing-zephyr-module) — drop a Zephyr model export next to the project and local inference is enabled automatically.

---

## How GATT works

**GATT** (Generic ATTribute Profile) is the protocol BLE devices use to exchange data once a connection is established. It sits on top of the low-level ATT (Attribute Protocol) and organises everything into a hierarchy of **Services** and **Characteristics**.

<img width="518" height="1242" alt="GATT profile hierarchy: Profile → Services → Characteristics" src="https://github.com/user-attachments/assets/fe9a57fe-0306-407d-a8ad-8bc32a3e7bcf" />

**Roles: Server and Client**

| Role | Who | What it does |
|------|-----|-------------|
| **GATT Server** | This firmware (Zephyr board) | Holds the ATT table — defines Services and Characteristics, responds to reads, sends notifications |
| **GATT Client** | Android app | Initiates all transactions — discovers services, reads values, subscribes to notifications |

A peripheral can only be connected to **one** central at a time. Once connected it stops advertising, so no other device can see it until the connection drops.

<img width="1948" height="1478" alt="BLE connected network topology: one central connected to multiple peripherals" src="https://github.com/user-attachments/assets/6af6454b-030f-477e-9087-448348477565" />

**Services and Characteristics in this firmware**

Each **Service** is identified by a 128-bit UUID and groups related **Characteristics** together. A Characteristic holds a single value (or a packed struct) and can be read directly or pushed to the client via *notifications* without the client polling.

<img width="3116" height="1858" alt="GATT transaction flow: client reads and subscribes, server notifies" src="https://github.com/user-attachments/assets/b0b822b3-55c0-4fea-8d77-d2d2ec1131bb" />

This firmware exposes one custom service with three characteristics:

| Characteristic | UUID suffix | Properties | Payload |
|---|---|---|---|
| Inference result | `…def1` | READ + NOTIFY | `inference_result_t` (52 bytes) |
| Sensor data | `…def2` | READ + NOTIFY | packed `float[]` |
| Device state | `…def3` | READ + WRITE | UTF-8 label string |

The Android app subscribes to the **Inference result** and **Sensor data** characteristics on connect (enabling notifications via the CCC descriptor), then receives a callback every time the firmware calls `gatt_server_notify_*()`.

> **References**
> - [Introduction to Bluetooth Low Energy — GATT](https://learn.adafruit.com/introduction-to-bluetooth-low-energy/gatt) — Kevin Townsend, Adafruit
> - [Bluetooth GATT Specification Supplement (GSS)](https://bitbucket.org/bluetooth-SIG/public/src/main/gss/) — Bluetooth SIG official characteristic and descriptor definitions
> - [Bluetooth Core Specification](https://www.bluetooth.com/specifications/specs/core-specification/) — Vol 3, Part G (ATT) and Part F (GATT) for the full protocol spec

---

## Supported boards

| Board | Zephyr ID | Mode | SoC |
|-------|-----------|------|-----|
| Nordic Thingy:53 | `thingy53/nrf5340/cpuapp` | Relay | nRF5340 (ARM Cortex-M33) |
| Arduino Nano 33 BLE Sense | `arduino_nano_33_ble` | Local sensor | nRF52840 (ARM Cortex-M4) |
| Arduino Nesso N1 | `arduino_nesso_n1` | Local sensor | ESP32-C6 (RISC-V) |

> **Nesso N1 note:** The `arduino_nesso_n1` board was added to Zephyr after v4.0.0.  If `west build` cannot find the board, update the `revision:` in `west.yml` to a newer Zephyr release and re-run `west update`.

Any board with an on-board IMU declared in the Zephyr devicetree can be used in local-sensor mode — add `boards/<board>.conf` with `CONFIG_EI_SENSOR_LOCAL=y` and a matching board overlay.

---

## Sensors

### Arduino Nano 33 BLE Sense

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

### Arduino Nesso N1

| Index | Sensor | Channel | Unit |
|-------|--------|---------|------|
| 0–2 | BMI270 accel | X, Y, Z | m/s² |
| 3–5 | BMI270 gyro | X, Y, Z | rad/s |

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

**Arduino Nesso N1 — local sensor mode (ESP32-C6)**
```bash
# Fetch ESP32 RF binary blobs once
west blobs fetch hal_espressif

west build --pristine -b arduino_nesso_n1 ei-zephyr-ble-gatt-client
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
│   ├── arduino_nano_33_ble.conf         # Sensor drivers + EI_SENSOR_LOCAL=y
│   ├── arduino_nesso_n1.overlay         # Placeholder (BMI270 already enabled)
│   └── arduino_nesso_n1.conf           # BMI270 + EI_SENSOR_LOCAL=y (ESP32-C6)
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
