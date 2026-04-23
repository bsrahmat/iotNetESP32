# Legacy Removal Checklist

Legacy cleanup has been completed.

## Removed

1. `IotNetESP32::begin()` no-argument overload.
2. Legacy global credential symbols:
   - `IOTNET_USERNAME`
   - `IOTNET_PASSWORD`
   - `IOTNET_BOARD_NAME`
3. Legacy credential usage in examples and README.

## Verification

1. All sketches use `IotNetESP32::ClientConfig`.
2. Native tests and ESP32 build pass after cleanup.
