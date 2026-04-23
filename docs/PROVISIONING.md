# ESP32 Provisioning Guide

> **How to configure ESP32 firmware with IoTNet credentials** obtained from the dashboard.
> This guide explains the correct values to use and common mistakes to avoid.

---

## Prerequisites

- IoTNet Dashboard account
- A created device with credentials (see [Device Provisioning Guide](../../server/docs/device-provisioning.md))

---

## Credentials Overview

When you create a device in IoTNet dashboard, you receive three critical values:

| Dashboard Field | Description | Example |
|----------------|-------------|---------|
| **Username** | Device UUID — used as MQTT username | `019cc382-0dac-70b1-98dc-1b81b3ab2c00` |
| **Password** | MQTT password (shown once only) | `a1b2c3d4e5f6...` (64 hex chars) |
| **Board ID** | Board identifier (brand_random30hex) | `espressif_fb2d6ad26fdf7b867baf41b8559a1c` |

**⚠️ Important**: Copy these values exactly as shown. Do not shorten, modify, or use custom values.

---

## Configuration

### Step 1: Get Credentials from Dashboard

1. Go to **Devices** in IoTNet Dashboard
2. Click on your created device
3. Copy the values from the credential modal:
   - **Username (Device ID)**: The UUID shown as username
   - **Password**: The hex string (click copy — shown only once)
   - **Board ID**: The board identifier string

### Step 2: Configure ESP32

Update your `ClientConfig` with exact values from dashboard:

```cpp
#include <IotNetESP32.h>

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// IoTNet credentials from dashboard
IotNetESP32::ClientConfig config = {
    // Use EXACT values from dashboard — do not use custom strings
    .mqttUsername = "019cc382-0dac-70b1-98dc-1b81b3ab2c00",  // Username field from dashboard
    .mqttPassword = "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef123456",  // Password field
    .boardIdentifier = "espressif_fb2d6ad26fdf7b867baf41b8559a1c",  // Board ID field
    .firmwareVersion = "v1.0.0",
    .enableOta = true
};

IotNetESP32 iotnet;

void setup() {
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Initialize IoTNet with credentials
    iotnet.begin(config);
}

void loop() {
    iotnet.run();

    // Your application code
    // Example: publish sensor data to V1
    // iotnet.virtualWrite("V1", sensorValue);
}
```

---

## Config Field Reference

| `ClientConfig` Field | Value Source | Must Match |
|---------------------|--------------|------------|
| `mqttUsername` | Dashboard → Username (Device ID) | Backend device UUID |
| `mqttPassword` | Dashboard → Password | Backend generated SHA256 hash |
| `boardIdentifier` | Dashboard → Board ID | Backend generated `{brand}_{random}` |
| `firmwareVersion` | Your choice | Any valid version string |
| `enableOta` | Your choice | `true` to enable OTA updates |

---

## MQTT Topic Format

The ESP32 publishes to this topic structure:

```
devices/{mqttUsername}/{boardIdentifier}/{channel}
```

| Segment | Value | Example |
|---------|-------|---------|
| `devices` | Fixed prefix | `devices` |
| `{mqttUsername}` | From `.mqttUsername` config | `019cc382-0dac-70b1-98dc-1b81b3ab2c00` |
| `{boardIdentifier}` | From `.boardIdentifier` config | `espressif_fb2d6ad26fdf7b867baf41b8559a1c` |
| `{channel}` | Virtual pin (V0-V49) or system | `V1`, `status`, `ota/update` |

### Example Topics

```
devices/019cc382-0dac-70b1-98dc-1b81b3ab2c00/espressif_fb2d6ad26fdf7b867baf41b8559a1c/V1
devices/019cc382-0dac-70b1-98dc-1b81b3ab2c00/espressif_fb2d6ad26fdf7b867baf41b8559a1c/status
devices/019cc382-0dac-70b1-98dc-1b81b3ab2c00/espressif_fb2d6ad26fdf7b867baf41b8559a1c/ota/update
```

---

## Virtual Pins

| Pin | Purpose | Direction | Notes |
|-----|---------|-----------|-------|
| V0 | Online/offline status | Pub/Sub | Auto-published on connect ("online") |
| V1-V49 | General purpose data | Pub/Sub | Use for sensor data, control signals |

### Publishing Data

```cpp
// Publish temperature to V1
float temperature = readTemperature();
iotnet.virtualWrite("V1", temperature);

// Publish switch state to V2
int switchState = digitalRead(SWITCH_PIN);
iotnet.virtualWrite("V2", switchState);
```

### Reading Incoming Data

```cpp
void handleV3Callback(String value) {
    Serial.printf("V3 received: %s\n", value.c_str());
    // Control something based on value
}

void setup() {
    // Register callback for V3
    iotnet.registerCallback("V3", handleV3Callback);
}
```

---

## Common Mistakes

### ❌ Wrong: Using Custom Strings

```cpp
// WRONG — these won't match the backend
.mqttUsername = "my-esp-device",
.boardIdentifier = "living-room",
```

**Why**: Backend expects exact UUID as username, and exact boardIdentifier as registered. Custom strings will fail EMQX authentication and topic subscription.

### ✅ Correct: Using Dashboard Values

```cpp
// CORRECT — use exact values from dashboard
.mqttUsername = "019cc382-0dac-70b1-98dc-1b81b3ab2c00",
.boardIdentifier = "espressif_fb2d6ad26fdf7b867baf41b8559a1c",
```

---

### ❌ Wrong: Truncated Password

```cpp
// WRONG — password must be full 64-char hex string
.mqttPassword = "a1b2c3d4e5f6",
```

**Why**: Backend generates 64-character SHA256 hash. Truncating makes password invalid.

### ✅ Correct: Full Password

```cpp
// CORRECT — use full 64-character hex string
.mqttPassword = "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef123456",
```

---

### ❌ Wrong: Board Name vs Board Identifier

```cpp
// WRONG — boardName is not the same as boardIdentifier
.boardName = "ESP32-DevKit",  // ❌ This is device TYPE name, not identifier

// CORRECT — use the Board ID from dashboard
.boardIdentifier = "espressif_fb2d6ad26fdf7b867baf41b8559a1c",  // ✅
```

---

## Verifying Connection

### Serial Monitor Output

After programming and running, you should see:

```
    ________  _______   ______________
   /  _/ __ \/_  __/ | / / ____/_  __/
   / // / / / / / /  |/ / __/   / /
 _/ // /_/ / / / / /|  / /___  / /
/___/\____/ /_/ /_/ |_/_____/ /_/

Firmware version: v1.0.0
[MQTT] Connected
```

If you see "Failed to connect to MQTT broker", check:
1. WiFi is connected
2. `mqttUsername` is exactly the UUID (copy from dashboard)
3. `mqttPassword` is correct (if unsure, reset key in dashboard)
4. `boardIdentifier` exactly matches Board ID from dashboard

### Dashboard Verification

1. Open IoTNet Dashboard → Devices → Select your device
2. Check Boards tab — your board should show "Online"
3. Go to Dashboards — widgets should receive data from ESP32

---

## If Credentials Are Lost

If you lost the password or ESP32 can't connect:

1. In Dashboard, go to Device → **Reset Key**
2. Backend generates new `mqttPassword`
3. Update ESP32 `mqttPassword` with the new value
4. Re-upload firmware to ESP32

---

## Related Documentation

- [IoTNet Terminology Guide](../../server/docs/terminology.md) — Term definitions
- [Device Provisioning Guide](../../server/docs/device-provisioning.md) — Full E2E provisioning flow
- [MQTT Topic Contract](../../server/docs/mqtt-topics.md) — Topic format specification

---

*Last updated: 2026-04-23*