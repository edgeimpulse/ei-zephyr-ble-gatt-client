# Edge Impulse BLE GATT Client for IMU Monitoring

A simple **BLE GATT client** that connects to Edge Impulse + Golioth devices and displays real-time IMU inference results via serial output. Perfect for monitoring ML-enabled IoT devices wirelessly.

**Companion to:** [ei-zephyr-golioth-integration](https://github.com/edgeimpulse/ei-zephyr-golioth-integration)

## Features

✅ **BLE Central Role** - Scans and connects to EI-Golioth peripheral devices  
✅ **GATT Client** - Discovers services and subscribes to inference notifications  
✅ **Serial Output** - Displays inference results via UART console  
✅ **Auto-reconnect** - Automatically rescans if connection drops  
✅ **Battery Powered** - Uses Thingy:53 with built-in battery  
✅ **Simple & Portable** - No display required, works anywhere  

## Hardware Requirements

### Client Device (Monitor)
**Nordic Thingy:53**
- nRF5340 SoC (BLE 5.3)
- Built-in 1350 mAh battery
- USB-C charging
- Compact & portable

### Peripheral Device (Data Source)
Any device running `ei-zephyr-golioth-integration`:
- nRF52840 DK (IMU-based)
- nRF5340 DK
- Any board with BLE + Edge Impulse IMU model

## Architecture

```
┌──────────────────┐         BLE GATT          ┌──────────────────┐
│ EI-Golioth       │◄────────────────────────►│ GATT Client      │
│ Peripheral       │    Inference Results      │ (Thingy:53)      │
│ (nRF52840)       │                           │                  │
│                  │                           │                  │
│ - Edge Impulse   │                           │ - BLE Central    │
│ - IMU Sensor     │                           │ - Serial Output  │
│ - Golioth SDK    │                           │ - Battery Power  │
└──────────────────┘                           └──────────────────┘
        │                                              │
        │ IMU: wave, idle, updown                      │ Displays via UART
        │ Streams via BLE                              │ Portable monitor
        ▼                                              ▼
    Local inference                            Serial console
```

## Initialize This Repo

```bash
west init -m https://github.com/edgeimpulse/ei-zephyr-ble-gatt-client.git
cd ei-zephyr-ble-gatt-client
west update
```

This fetches:
- Zephyr RTOS
- BLE stack (nRF5340 controller)

## Build

For Nordic Thingy:53:

```bash
west build --pristine -b thingy53/nrf5340/cpuapp
```

## Flash

```bash
west flash
```

## Project Structure

```
├── CMakeLists.txt          # Build configuration
├── prj.conf                # BLE central config (no display)
├── west.yml                # Manifest
├── boards/
│   └── thingy53_nrf5340_cpuapp.overlay  # UART config
└── src/
    ├── main.cpp            # Application entry point
    └── ble/
        ├── gatt_client.h   # GATT client API
        └── gatt_client.cpp # BLE scanning, connection, discovery
```

## How It Works

### 1. **Initialization**
```
========================================
  Edge Impulse BLE GATT Client
  Monitoring EI-Golioth IMU Devices
========================================

Scanning for EI-Golioth devices...
```

### 2. **Scanning & Connection**
```
Device found: EI-Golioth (RSSI: -45 dBm)
Connecting...

>>> Connected to EI-Golioth device!
```

### 3. **Subscribe to Notifications**
```
Service discovered: Edge Impulse (UUID: 0x1234...)
Characteristic: Inference Results
Subscribing to notifications...
```

### 4. **Receive Inference Results**
```
=== Inference Result ===
Label: wave
Confidence: 97.0%
DSP Time: 47 ms
Classification Time: 6 ms
Total Time: 53 ms
========================
```

### 5. **Auto-Reconnect on Disconnect**
```
>>> Disconnected from device
>>> Rescanning in 2 seconds...

Scanning for EI-Golioth devices...
```

## Configuration

### BLE Settings (`prj.conf`)

```properties
# BLE Central
CONFIG_BT=y
CONFIG_BT_CENTRAL=y
CONFIG_BT_GATT_CLIENT=y
CONFIG_BT_DEVICE_NAME="EI-Monitor"

# Scanning
CONFIG_BT_SCAN=y
CONFIG_BT_SCAN_FILTER_ENABLE=y

# Connection
CONFIG_BT_MAX_CONN=1
CONFIG_BT_L2CAP_TX_MTU=247
```

### Serial Output

```properties
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y
CONFIG_CONSOLE=y
CONFIG_PRINTK=y
```

### Memory Configuration

```properties
CONFIG_HEAP_MEM_POOL_SIZE=32768
CONFIG_MAIN_STACK_SIZE=8192
```

## BLE GATT Service Details

### Service UUID
```
12345678-1234-5678-1234-56789abcdef0
```

### Characteristics

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Inference | ...def1 | READ, NOTIFY | ML inference results |
| Sensor Data | ...def2 | READ | Raw sensor data |
| Device State | ...def3 | READ, WRITE | Device status |

### Inference Data Format

```cpp
struct inference_result_t {
    char label[32];              // Classification label
    float confidence;            // 0.0 - 1.0
    uint32_t dsp_time_ms;       // DSP processing time
    uint32_t classification_time_ms;  // Classification time
    uint64_t timestamp;          // Unix timestamp
};
```

## Usage with ei-zephyr-golioth-integration

### Step 1: Flash Peripheral Device (IMU Source)

On nRF52840 DK with IMU sensor:
```bash
cd ei-zephyr-golioth-integration
west build -b nrf52840dk_nrf52840
west flash
```

### Step 2: Flash GATT Client (Monitor)

On Thingy:53:
```bash
cd ei-zephyr-ble-gatt-client
west build -b thingy53/nrf5340/cpuapp
west flash
```

### Step 3: Connect Serial Console

```bash
# Find the Thingy:53 serial port
ls /dev/tty.usbmodem*

# Connect with screen (macOS/Linux)
screen /dev/tty.usbmodem14401 115200

# Or use minicom
minicom -D /dev/tty.usbmodem14401 -b 115200
```

### Step 4: Power On & Monitor

The Thingy:53 will automatically:
1. Scan for "EI-Golioth" device
2. Connect when found
3. Subscribe to notifications
4. Display results in real-time

**Serial output:**
```
========================================
  Edge Impulse BLE GATT Client
  Monitoring EI-Golioth IMU Devices
========================================

Scanning for EI-Golioth devices...

Device found: EI-Golioth (RSSI: -45 dBm)

>>> Connected to EI-Golioth device!

=== Inference Result ===
Label: wave
Confidence: 97.0%
DSP Time: 47 ms
Classification Time: 6 ms
Total Time: 53 ms
========================

=== Inference Result ===
Label: idle
Confidence: 89.5%
DSP Time: 48 ms
Classification Time: 6 ms
Total Time: 54 ms
========================
```

## GATT Client API

### Initialization

```cpp
#include "ble/gatt_client.h"

// Initialize BLE
gatt_client_init();
```

### Scanning and Connection

```cpp
// Start scanning for "EI-Golioth" devices
gatt_client_start_scan();

// Check connection status
if (gatt_client_is_connected()) {
    // Connected
}
```

### Register Callbacks

```cpp
// Handle inference results
void on_inference(const inference_result_t *result) {
    printk("Label: %s, Confidence: %.1f%%\n", 
           result->label, result->confidence * 100.0f);
}

gatt_client_register_inference_callback(on_inference);

// Handle connection changes
void on_connection(bool connected) {
    if (connected) {
        printk("Connected!\n");
    }
}

gatt_client_register_connection_callback(on_connection);
```

### Enable Notifications

```cpp
// Subscribe to inference result notifications
gatt_client_enable_inference_notifications();
```

## Why Thingy:53?

### ✅ **Built-in Battery**
- 1350 mAh rechargeable battery
- USB-C charging
- Truly portable monitoring

### ✅ **Compact Size**
- Small form factor
- Enclosure included
- Easy to carry

### ✅ **BLE 5.3**
- Latest Bluetooth standard
- Longer range
- Lower power

### ✅ **USB Console**
- USB-C for serial output
- No external debugger needed
- Simple plug-and-play

## Troubleshooting

### Device Not Found

**Problem:** "Scanning..." never finds device

**Solutions:**
- Verify peripheral is advertising as "EI-Golioth"
- Check peripheral is powered and running
- Ensure Thingy:53 battery is charged
- Check RSSI range (should be < -70 dBm)
- Enable debug logging:
  ```properties
  CONFIG_BT_LOG_LEVEL_DBG=y
  ```

### Connection Drops

**Problem:** Frequent disconnections

**Solutions:**
- Reduce distance between devices
- Check for BLE interference
- Verify Thingy:53 battery level
- Check peripheral is not entering low power mode

### No Serial Output

**Problem:** No console output visible

**Solutions:**
- Verify USB-C cable is data-capable
- Check correct serial port selected
- Try resetting Thingy:53
- Verify baud rate is 115200

### No Notifications Received

**Problem:** Connected but no inference data

**Solutions:**
- Check service UUIDs match on both devices
- Verify peripheral is sending notifications
- Enable GATT logging:
  ```properties
  CONFIG_BT_GATT_LOG_LEVEL_DBG=y
  ```
- Manually trigger inference on peripheral (perform gesture)

## Performance Optimization

### Reduce Update Latency

```properties
# Faster BLE connection
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=12

# Higher MTU
CONFIG_BT_L2CAP_TX_MTU=247
```

### Extend Battery Life

```properties
# Enable power management
CONFIG_PM=y
CONFIG_PM_DEVICE=y

# Optimize BLE power
CONFIG_BT_CTLR_TX_PWR_0=y  # 0 dBm
```

### Reduce Serial Output

Comment out verbose logging in `main.cpp`:
```cpp
// printk("========================\n\n");  // Less output
```

## Advanced Features

### Log to SD Card

Add data logging (requires SD card shield):

```cpp
#include <zephyr/fs/fs.h>

void log_inference(const inference_result_t *result) {
    struct fs_file_t file;
    fs_file_t_init(&file);
    fs_open(&file, "/sd/inference.log", FS_O_APPEND);
    // Write result with timestamp
    fs_close(&file);
}
```

### Multiple Device Support

Modify to monitor multiple peripherals:

```cpp
#define MAX_DEVICES 3
static struct bt_conn *connections[MAX_DEVICES];
```

### Timestamp Logging

Add timestamps to inference output:

```cpp
#include <zephyr/sys/timeutil.h>

uint64_t timestamp = k_uptime_get();
printk("Timestamp: %llu ms\n", timestamp);
```

## Integration Examples

### With Python Script

Parse serial output with Python:

```python
import serial

ser = serial.Serial('/dev/tty.usbmodem14401', 115200)

while True:
    line = ser.readline().decode('utf-8').strip()
    if "Label:" in line:
        label = line.split(": ")[1]
        print(f"Detected: {label}")
```

### With Node-RED

1. Use serial node to read UART
2. Parse inference data
3. Send to dashboard/database

### CSV Logging

```bash
screen -L -Logfile inference.log /dev/tty.usbmodem14401 115200
# Press Ctrl-A, then D to detach
# Log continues in background
```

## Typical IMU Gestures

When connected to an IMU-based EI-Golioth device, you'll see classifications like:

- **wave** - Side-to-side waving motion
- **idle** - Device at rest
- **updown** - Up and down motion
- **circle** - Circular motion
- **Custom gestures** - Based on your Edge Impulse model

## Example Session

```
========================================
  Edge Impulse BLE GATT Client
  Monitoring EI-Golioth IMU Devices
========================================

Scanning for EI-Golioth devices...

Device found: EI-Golioth (RSSI: -42 dBm)

>>> Connected to EI-Golioth device!

=== Inference Result ===
Label: idle
Confidence: 95.2%
DSP Time: 45 ms
Classification Time: 5 ms
Total Time: 50 ms
========================

=== Inference Result ===
Label: wave
Confidence: 98.1%
DSP Time: 46 ms
Classification Time: 6 ms
Total Time: 52 ms
========================

=== Inference Result ===
Label: updown
Confidence: 87.3%
DSP Time: 47 ms
Classification Time: 6 ms
Total Time: 53 ms
========================
```

## Battery Life

**Thingy:53 Battery (1350 mAh):**
- Active monitoring: ~24-48 hours
- With power optimization: 48-72 hours
- Charging time: ~2 hours via USB-C

**Tips for extending battery:**
```properties
# Reduce TX power
CONFIG_BT_CTLR_TX_PWR_MINUS_4=y

# Longer connection intervals
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=80
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=120
```

## Resources

- [Base Golioth Integration](https://github.com/edgeimpulse/ei-zephyr-golioth-integration)
- [Zephyr BLE Documentation](https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html)
- [Nordic Thingy:53](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-53)
- [Edge Impulse Docs](https://docs.edgeimpulse.com/)

## License

Clear BSD License - see `LICENSE` file  
Copyright (c) 2025 EdgeImpulse Inc.
```

## Initialize This Repo

```bash
west init -m https://github.com/edgeimpulse/ei-zephyr-ble-gatt-client.git
cd ei-zephyr-ble-gatt-client
west update
```

This fetches:
- Zephyr RTOS
- BLE stack
- LVGL graphics library
- Display drivers

## Build

For XIAO ESP32-S3:

```bash
west build --pristine -b xiao_esp32s3/esp32s3/procpu
```

## Flash

```bash
west flash
```

## Project Structure

```
├── CMakeLists.txt          # Build configuration
├── prj.conf                # BLE central + display config
├── west.yml                # Manifest
├── boards/
│   └── xiao_esp32s3_procpu.overlay  # Display pin config
└── src/
  ├── main.cpp              # Application entry point
  ├── ble/
  │   ├── gatt_client.h     # GATT client API
  │   └── gatt_client.cpp   # BLE scanning, connection, discovery
  └── display/
      ├── ei_display.h      # Display interface
      └── ei_display.cpp    # LVGL integration
```

## How It Works

### 1. **Initialization**
- Initialize BLE in central mode
- Initialize round display with LVGL
- Show "Scanning..." status

### 2. **Scanning**
```
Scanning for: EI-Golioth
Device found: EI-Golioth (RSSI: -45 dBm)
Connecting...
```

### 3. **Connection**
- Automatic connection when target device found
- GATT service discovery
- Find Edge Impulse service UUID
- Locate inference characteristic

### 4. **Subscribe to Notifications**
```
Service discovered: Edge Impulse (UUID: 0x1234...)
Characteristic: Inference Results
Subscribing to notifications...
```

### 5. **Receive Inference Results**
```json
{
  "label": "wave",
  "confidence": 0.97,
  "dsp_time_ms": 47,
  "classification_time_ms": 6
}
```

### 6. **Display on Screen**
```
┌─────────────────────┐
│   EI Monitor        │
│                     │
│      wave           │
│     97.0% ●         │  (green)
│      53 ms          │
└─────────────────────┘
```

## Configuration

### BLE Settings (`prj.conf`)

```properties
# BLE Central
CONFIG_BT=y
CONFIG_BT_CENTRAL=y
CONFIG_BT_GATT_CLIENT=y
CONFIG_BT_DEVICE_NAME="EI-Monitor"

# Scanning
CONFIG_BT_SCAN=y
CONFIG_BT_SCAN_FILTER_ENABLE=y

# Connection
CONFIG_BT_MAX_CONN=1
CONFIG_BT_L2CAP_TX_MTU=247
```

### Display Settings

```properties
# Display
CONFIG_DISPLAY=y
CONFIG_GC9A01=y

# LVGL
CONFIG_LVGL=y
CONFIG_LV_Z_MEM_POOL_SIZE=16384
CONFIG_LV_Z_DOUBLE_VDB=y
```

### Memory Configuration

```properties
CONFIG_HEAP_MEM_POOL_SIZE=131072
CONFIG_MAIN_STACK_SIZE=16384
```

## BLE GATT Service Details

### Service UUID
```
12345678-1234-5678-1234-56789abcdef0
```

### Characteristics

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Inference | ...def1 | READ, NOTIFY | ML inference results |
| Sensor Data | ...def2 | READ | Raw sensor data |
| Device State | ...def3 | READ, WRITE | Device status |

### Inference Data Format

```cpp
struct inference_result_t {
    char label[32];              // Classification label
    float confidence;            // 0.0 - 1.0
    uint32_t dsp_time_ms;       // DSP processing time
    uint32_t classification_time_ms;  // Classification time
    uint64_t timestamp;          // Unix timestamp
};
```

## Display Output

### Connection States

**Scanning:**
```
┌─────────────────────┐
│   EI Monitor        │
│                     │
│   Initializing...   │
│                     │
└─────────────────────┘
```

**Connected:**
```
┌─────────────────────┐
│   EI Monitor        │
│                     │
│    Connected!       │  (green)
│                     │
└─────────────────────┘
```

**Receiving Data:**
```
┌─────────────────────┐
│   EI Monitor        │
│                     │
│      updown         │
│     85.3% ●         │  (orange)
│      48 ms          │
└─────────────────────┘
```

**Disconnected:**
```
┌─────────────────────┐
│   EI Monitor        │
│                     │
│   Disconnected      │  (red)
│    Scanning...      │
└─────────────────────┘
```

## GATT Client API

### Initialization

```cpp
#include "ble/gatt_client.h"

// Initialize BLE
gatt_client_init();
```

### Scanning and Connection

```cpp
// Start scanning for "EI-Golioth" devices
gatt_client_start_scan();

// Or connect to specific device
gatt_client_connect("EI-Golioth");

// Check connection status
if (gatt_client_is_connected()) {
    // Connected
}

// Disconnect
gatt_client_disconnect();
```

### Register Callbacks

```cpp
// Handle inference results
void on_inference(const inference_result_t *result) {
    printf("Label: %s, Confidence: %.2f%%\n", 
           result->label, result->confidence * 100.0f);
}

gatt_client_register_inference_callback(on_inference);

// Handle connection changes
void on_connection(bool connected) {
    if (connected) {
        printf("Connected to device\n");
    }
}

gatt_client_register_connection_callback(on_connection);
```

### Enable Notifications

```cpp
// Subscribe to inference result notifications
gatt_client_enable_inference_notifications();
```

## Usage with ei-zephyr-golioth-integration

### Step 1: Flash Peripheral Device

On nRF52840 DK (or other peripheral):
```bash
cd ei-zephyr-golioth-integration
west build -b nrf52840dk_nrf52840
west flash
```

### Step 2: Flash GATT Client

On XIAO ESP32-S3 (display monitor):
```bash
cd ei-zephyr-ble-gatt-client
west build -b xiao_esp32s3/esp32s3/procpu
west flash
```

### Step 3: Power On Both Devices

The client will automatically:
1. Scan for "EI-Golioth" device
2. Connect when found
3. Discover GATT services
4. Subscribe to notifications
5. Display results in real-time

### Step 4: Monitor Serial Output

**Client (XIAO ESP32-S3):**
```
=== Edge Impulse BLE GATT Client ===
Bluetooth initialized
Scanning for EI-Golioth...
Found EI-Golioth (RSSI: -45 dBm)
Connected
Started GATT discovery
Found EI service
Found inference characteristic
Subscribed to inference notifications

=== Inference Result ===
Label: wave
Confidence: 97.0%
DSP Time: 47 ms
Classification Time: 6 ms
```

**Peripheral (nRF52840):**
```
=== EI-Golioth Integration ===
Golioth client connected
Edge Impulse initialized

Predictions (DSP: 47 ms, Classification: 6 ms):
    idle: 0.02
    wave: 0.97
    updown: 0.01

BLE notification sent
```

## Customization

### Change Target Device Name

Edit `src/ble/gatt_client.cpp`:
```cpp
static const char *target_device_name = "My-Custom-Device";
```

### Adjust Display Layout

Edit `src/main.cpp`:
```cpp
lv_obj_t *title = lv_label_create(lv_scr_act());
lv_label_set_text(title, "Custom Title");
lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
```

### Modify Confidence Thresholds

Edit `src/display/ei_display.cpp`:
```cpp
if (confidence > 0.90f) {
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_GREEN), 0);
} else if (confidence > 0.70f) {
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_ORANGE), 0);
}
```

## Troubleshooting

### Device Not Found

**Problem:** "Scanning..." never finds device

**Solutions:**
- Verify peripheral is advertising as "EI-Golioth"
- Check peripheral is powered and running
- Ensure BLE is enabled on peripheral
- Check RSSI range (should be < -70 dBm)
- Enable debug logging:
  ```properties
  CONFIG_BT_LOG_LEVEL_DBG=y
  ```

### Connection Drops

**Problem:** Frequent disconnections

**Solutions:**
- Reduce distance between devices
- Check for BLE interference
- Increase connection interval:
  ```c
  BT_LE_CONN_PARAM(6, 12, 0, 400)  // Slower but more stable
  ```
- Verify peripheral is not entering low power mode

### No Notifications Received

**Problem:** Connected but no data on display

**Solutions:**
- Check service UUIDs match on both devices
- Verify peripheral is sending notifications
- Enable GATT logging:
  ```properties
  CONFIG_BT_GATT_LOG_LEVEL_DBG=y
  ```
- Manually trigger inference on peripheral

### Display Not Working

**Problem:** Black screen or no updates

**Solutions:**
- Check SPI connections
- Verify display power (3.3V)
- Enable display logging:
  ```properties
  CONFIG_DISPLAY_LOG_LEVEL_DBG=y
  CONFIG_SPI_LOG_LEVEL_DBG=y
  ```
- Test with simpler LVGL example

### Memory Issues

**Problem:** Heap allocation failures

**Solutions:**
```properties
# Increase heap size
CONFIG_HEAP_MEM_POOL_SIZE=262144

# Increase main stack
CONFIG_MAIN_STACK_SIZE=32768

# Reduce LVGL memory
CONFIG_LV_Z_MEM_POOL_SIZE=8192
```

## Performance Optimization

### Reduce Update Latency

```properties
# Faster BLE connection
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=12

# Higher MTU
CONFIG_BT_L2CAP_TX_MTU=247
```

### Optimize Display Refresh

```cpp
// Update display only when data changes
static inference_result_t last_result;
if (memcmp(&last_result, result, sizeof(*result)) != 0) {
    update_display(result);
    last_result = *result;
}
```

### Power Saving

```properties
# Enable low power mode
CONFIG_PM=y
CONFIG_PM_DEVICE=y

# BLE power optimization
CONFIG_BT_CTLR_TX_PWR_0=y  # 0 dBm transmit power
```

## Advanced Features

### Multiple Device Support

Modify to connect to multiple peripherals:

```cpp
#define MAX_DEVICES 3
static struct bt_conn *connections[MAX_DEVICES];
```

### Data Logging

Add SD card logging:

```cpp
#include <zephyr/fs/fs.h>

void log_inference(const inference_result_t *result) {
    struct fs_file_t file;
    fs_file_t_init(&file);
    fs_open(&file, "/sd/inference.log", FS_O_APPEND);
    // Write result
    fs_close(&file);
}
```

### Remote Control

Add write characteristic for control commands:

```cpp
int send_calibrate_command(void) {
    uint8_t cmd = 0x01;  // Calibrate
    bt_gatt_write_without_response(conn, cmd_handle, &cmd, 1, false);
}
```

## Integration Examples

### With Node-RED

Send BLE data to Node-RED dashboard:
1. Add WiFi to XIAO ESP32-S3
2. Send HTTP POST with inference data
3. Display on Node-RED dashboard

### With Home Assistant

Integrate as BLE sensor:
```yaml
sensor:
  - platform: ble
    mac: "AA:BB:CC:DD:EE:FF"
    name: "EI Inference"
```

### With Mobile App

Use Nordic nRF Connect or custom React Native app to monitor data.

## Resources

- [Base Golioth Integration](https://github.com/edgeimpulse/ei-zephyr-golioth-integration)
- [Zephyr BLE Documentation](https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [Seeed XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)

## License

Clear BSD License - see `LICENSE` file  
Copyright (c) 2025 EdgeImpulse Inc.
