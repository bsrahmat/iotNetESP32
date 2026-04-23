#!/bin/bash

DEVICE="/dev/ttyUSB0"
BAUD="115200"
LOG_FILE="esp32.log"

echo "=== Serial Log Started: $(date) ===" > "$LOG_FILE"
echo "Monitoring $DEVICE at $BAUD baud... (Ctrl+C to stop)"
echo "Log file: $LOG_FILE"

cd /home/farismnrr/Documents/Programs/IoTNet/plugins/iotNetESP32
pio device monitor --port "$DEVICE" --baud "$BAUD" 2>&1 | tee "$LOG_FILE"
