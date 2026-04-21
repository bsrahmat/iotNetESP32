# IoTNet ESP32 E2E Test Plan

**Test Environment:**
- ESP32: `/dev/ttyUSB0` (connected)
- BE API: `https://api.i-ot.net`
- FE: `https://i-ot.net`
- Test User: `john.doe@example.com` / `*Test12345`

---

## ESP32-E2E-001: Device Registration via MQTT → Verified via BE API

**Purpose:** ESP32 connects to MQTT broker, publishes board info, data appears in BE

### Step 1: Create test device via BE API

```bash
# Login to get token
TOKEN=$(curl -s -X POST https://api.i-ot.net/v1/users/login \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d '{"email":"john.doe@example.com","password":"*Test12345"}' | jq -r '.data.access_token')

# Create device
DEVICE_RESPONSE=$(curl -s -X POST https://api.i-ot.net/v1/devices \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d '{"name":"ESP32-E2E-TEST","device_type_id":"02d8f3c1-a4e9-4d14-b22f-f3f3a2e4e3ea"}')

DEVICE_ID=$(echo $DEVICE_RESPONSE | jq -r '.data.deviceId')
BOARD_ID=$(echo $DEVICE_RESPONSE | jq -r '.data.boardId')
echo "DEVICE_ID=$DEVICE_ID, BOARD_ID=$BOARD_ID"

# Get MQTT credentials
MQTT_CREDS=$(curl -s https://api.i-ot.net/v1/devices/$DEVICE_ID/key \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f")

USERNAME=$(echo $MQTT_CREDS | jq -r '.data.username')
PASSWORD=$(echo $MQTT_CREDS | jq -r '.data.password')
BOARD_NAME=$(echo $MQTT_CREDS | jq -r '.data.board_identifier')
echo "MQTT USERNAME=$USERNAME"
echo "MQTT BOARD_NAME=$BOARD_NAME"
```

### Step 2: Flash ESP32 with credentials

Edit `examples/IotNetOtaRemovalTest/IotNetOtaRemovalTest.ino` with values from Step 1, then:

```bash
cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32
pio run -e esp32doit-devkit-v1 -t upload --upload-port /dev/ttyUSB0
```

### Step 3: Monitor serial output

```bash
pio device monitor --port /dev/ttyUSB0 --speed 115200
```

**Expected serial output:**
```
[✅ PASS] [WIFI] Connected successfully! IP: 192.168.x.x
[✅ PASS] [MQTT_CONNECT] MQTT broker connection established
[✅ PASS] [VIRTUAL_PIN_PUBLISH] Published V1 = test_1
[✅ PASS] [VIRTUAL_PIN_PUBLISH] Published V1 = test_2
[✅ PASS] [VIRTUAL_PIN_PUBLISH] Published V1 = test_3
[✅ PASS] [BOARD_STATUS] Board status published (online)
```

### Step 4: Verify via BE API

```bash
# Check device status
curl -s https://api.i-ot.net/v1/devices/$DEVICE_ID \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" | jq

# Expected: device.status = "active"
```

### Step 5: Verify via FE Browser

1. Open https://i-ot.net/login
2. Login with `john.doe@example.com` / `*Test12345`
3. Navigate to Devices → ESP32-E2E-TEST
4. **Expected:** Device shows "online" status
5. Navigate to Boards tab
6. **Expected:** Board identifier matches, firmware version displayed

**Pass Criteria:** Serial shows `[✅ PASS]` for all phases AND BE API returns device status "active" AND FE shows device online

---

## ESP32-E2E-002: Virtual Pin Pub/Sub via Dashboard

**Purpose:** ESP32 publishes sensor data, user controls ESP32 via dashboard widget

**Prerequisites:** ESP32-E2E-001 completed, ESP32 running

### Step 1: Get access token

```bash
TOKEN=$(curl -s -X POST https://api.i-ot.net/v1/users/login \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d '{"email":"john.doe@example.com","password":"*Test12345"}' | jq -r '.data.access_token')
```

### Step 2: Create dashboard for ESP32 device

```bash
DEVICE_ID="<DEVICE_ID from ESP32-E2E-001>"

# Get device boards
BOARDS=$(curl -s https://api.i-ot.net/v1/devices/$DEVICE_ID/boards \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f")
BOARD_ID=$(echo $BOARDS | jq -r '.data.device_boards[0].id')

# Create dashboard
DASH_RESPONSE=$(curl -s -X POST https://api.i-ot.net/v1/dashboards \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d "{\"name\":\"ESP32-E2E-Dashboard\",\"device_board_id\":\"$BOARD_ID\"}")

DASHBOARD_ID=$(echo $DASH_RESPONSE | jq -r '.data.dashboard_id')
echo "DASHBOARD_ID=$DASHBOARD_ID"

# Get pins for the board
PINS=$(curl -s https://api.i-ot.net/v1/dashboards/$DASHBOARD_ID/pins \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f')
echo "Available pins:"
echo $PINS | jq '.data.dashboard_pins[] | {pin_name, pin_index}'
```

### Step 3: Create widgets for V1 and V2

```bash
V1_PIN_ID=$(echo $PINS | jq -r '.data.dashboard_pins[] | select(.pin_name=="V1") | .id')
V2_PIN_ID=$(echo $PINS | jq -r '.data.dashboard_pins[] | select(.pin_name=="V2") | .id')

# Get item types
ITEMS=$(curl -s https://api.i-ot.net/v1/dashboards/$DASHBOARD_ID/items \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f')

# Create stat widget for V1 (display published counter)
STAT_ITEM_ID=$(echo $ITEMS | jq -r '.data.dashboard_items[] | select(.type=="stat") | .id')
curl -s -X POST https://api.i-ot.net/v1/dashboards/$DASHBOARD_ID/widgets \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d "{\"dashboard_pin_id\":\"$V1_PIN_ID\",\"dashboard_item_id\":\"$STAT_ITEM_ID\",\"name\":\"V1 Counter\",\"settings\":{}}" | jq

# Create switch widget for V2 (control LED)
SWITCH_ITEM_ID=$(echo $ITEMS | jq -r '.data.dashboard_items[] | select(.type=="switch") | .id')
curl -s -X POST https://api.i-ot.net/v1/dashboards/$DASHBOARD_ID/widgets \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d "{\"dashboard_pin_id\":\"$V2_PIN_ID\",\"dashboard_item_id\":\"$SWITCH_ITEM_ID\",\"name\":\"V2 LED Control\",\"settings\":{}}" | jq
```

### Step 4: Open FE Dashboard and verify

1. Open https://i-ot.net/dashboards/$DASHBOARD_ID
2. **Expected:** V1 widget shows incrementing counter (ESP32 publishing every 3s)
3. Toggle V2 switch widget to "ON" (value=1)

### Step 5: Verify ESP32 received V2 command

**Check serial output:**
```
[✅ PASS] [VIRTUAL_PIN_SUBSCRIBE] Received V2 value: 1
```

### Step 6: Physical verification

- LED on ESP32-DevKitC should turn ON

**Pass Criteria:**
- V1 counter increments in FE dashboard
- ESP32 serial shows V2 callback with value `1`
- Physical LED on ESP32-DevKitC lights up

---

## ESP32-E2E-003: OTA Firmware Update

**Purpose:** Push new firmware to ESP32 via BE → MQTT → download → flash → reboot

**Prerequisites:** ESP32-E2E-001 completed, ESP32 running with current firmware

### Step 1: Get credentials

```bash
TOKEN=$(curl -s -X POST https://api.i-ot.net/v1/users/login \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -d '{"email":"john.doe@example.com","password":"*Test12345"}' | jq -r '.data.access_token')

DEVICE_ID="<DEVICE_ID from ESP32-E2E-001>"
BOARD_ID="<BOARD_ID from ESP32-E2E-001>"
```

### Step 2: Build new firmware version

```bash
cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32

# Build firmware (version is hardcoded in sketch as "1.0.0-test")
pio run -e esp32doit-devkit-v1 -t build

# Get firmware path
FIRMWARE_PATH=".pio/build/esp32doit-devkit-v1/firmware.bin"
ls -la $FIRMWARE_PATH
```

### Step 3: Upload firmware to BE

```bash
# Upload firmware binary
curl -s -X POST "https://api.i-ot.net/v1/devices/$DEVICE_ID/versions" \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" \
  -F "file=@.pio/build/esp32doit-devkit-v1/firmware.bin" \
  -F "version=1.0.99-e2e" | jq

# Verify uploaded
curl -s "https://api.i-ot.net/v1/devices/$DEVICE_ID/versions" \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" | jq
```

### Step 4: Trigger OTA from BE

```bash
# Publish OTA to device via MQTT
curl -s -X POST "https://api.i-ot.net/v1/devices/$DEVICE_ID/boards/$BOARD_ID/ota" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" | jq

# Response should be: {"status":"success"}
```

### Step 5: Monitor ESP32 serial output

```bash
pio device monitor --port /dev/ttyUSB0 --speed 115200
```

**Expected serial output:**
```
[OTA] Received OTA message: {"nonce":123456,"version":"1.0.99-e2e","url":"https://storage.i-ot.net/..."}
[OTA] Downloading firmware from: https://storage.i-ot.net/...
[OTA] Download size: 974576 bytes
[OTA] Free heap before update: 185496 bytes
[OTA] Download complete: 974576 bytes written
[OTA] Firmware flashed successfully
[OTA] Update successful! Rebooting...

    ________  _______   ______________
   /  _/ __ \/_  __/ | / / ____/_  __/
  / // / / / / / /  |/ / __/   / /
 _/ // /_/ / / / /|  / /___  / /
/___/\____/ /_/ /_/|_/_____/ /_/
Firmware version: 1.0.99-e2e
[✅ PASS] [WIFI] Connected successfully!
[✅ PASS] [MQTT_CONNECT] MQTT broker connection established
```

### Step 6: Verify via BE API

```bash
# Check firmware version updated
curl -s https://api.i-ot.net/v1/devices/$DEVICE_ID/boards/$BOARD_ID \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-API-Key: d225c7368604b14739f1030b2d48622adef2fad29f4b8721ef6b29c3cd81296f" | jq '.data.device_board.current_firmware_version'

# Expected: "1.0.99-e2e"
```

### Step 7: Verify via FE Dashboard

1. Open https://i-ot.net/devices/$DEVICE_ID
2. Navigate to Boards tab
3. **Expected:** Firmware version shows "1.0.99-e2e"

**Pass Criteria:**
- ESP32 reboots with new firmware version in serial output
- BE API returns updated firmware version
- FE Dashboard shows new firmware version

---

## ESP32-E2E-004: OTA Failure Handling (Invalid URL)

**Purpose:** ESP32 gracefully handles OTA failure and continues running current firmware

### Step 1: Trigger OTA (let it fail naturally by uploading bad firmware or invalid URL)

### Step 2: Monitor ESP32 serial output

**Expected:**
```
[OTA] Received OTA message: {"nonce":..., "version":"1.0.99-bad", "url":"..."}
[OTA] Downloading firmware from: https://storage.i-ot.net/invalid/broken.bin
[OTA] HTTP GET failed with code: -1
[OTA] Update failed. Continuing with current firmware.
Update Status: failed (Version: 1.0.99-e2e)
```

### Step 3: Verify ESP32 continues operating

- ESP32 remains connected to MQTT
- V1 continues publishing counter values
- V2 remains responsive to dashboard commands

**Pass Criteria:** ESP32 publishes `status=failed` to MQTT and continues normal operation without reboot

---

## Test Summary

| Test ID | Scope | Key Validations |
|---------|-------|----------------|
| ESP32-E2E-001 | Device Registration | Serial `[✅ PASS]` + BE API device status + FE UI online |
| ESP32-E2E-002 | Virtual Pins | V1 counter in FE + V2 command received in serial + LED physical |
| ESP32-E2E-003 | OTA Update | Serial shows download+flash + BE firmware version updated + FE shows new version |
| ESP32-E2E-004 | OTA Failure | Serial shows error + ESP32 continues operating |