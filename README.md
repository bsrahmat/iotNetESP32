# i-ot.net Library

IotNetESP32 is a library for ESP32 that simplifies IoT development by providing pre-configured WiFi, MQTT, and HTTP client functionalities, enabling seamless integration with `https://i-ot.net` platforms.

## Features

- **WiFi Connectivity Management**: Efficiently manages WiFi connections to ensure the device stays connected to the network.
- **Secure MQTT Communication**: Supports encrypted MQTT communication for secure data transfer.
- **HTTP Client with SSL Support**: Provides HTTP client functionality with SSL support for secure interactions.
- **Virtual Pin System**: Enables device interaction through a virtual pin system, supporting up to 20 virtual pins.
- **Real-Time Dashboard Integration**: Integrates with a real-time dashboard for data visualization and monitoring.
- **Over-The-Air (OTA) Firmware Updates**: Allows wireless firmware updates for easy device maintenance.
- **JSON Data Handling**: Processes data in JSON format for high compatibility and flexibility.
- **Thread-Safe State Management**: Ensures thread-safe management of device states to maintain stability and reliability.
- **Automatic Device Validation and Status Updates**: Automatically validates devices and updates their status for seamless operation.

## Installation

To use the IotNetESP32 library in your Arduino project, you have two options:

### Using Library Manager (Recommended)

- Open your Arduino IDE
- Go to `Sketch` -> `Include Library` -> `Manage Libraries`
- Search for `IotNetESP32` and install it

### Manual Installation

- Download the ZIP file from this repository
- Open Arduino IDE, select `Sketch` > `Include Library` > `Add .ZIP Library...` and select the downloaded ZIP file
- After installation, the IotNetESP32 library can be used in your project

## Usage

To use the IotNetESP32 library in your project, you need to include the library in your sketch and initialize the library with your device credentials.

```cpp
#include <IotNetESP32.h>

// WiFi Credentials
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// MQTT Credentials
constexpr char MQTT_USERNAME[] = "YOUR_MQTT_USERNAME";
constexpr char MQTT_PASSWORD[] = "YOUR_MQTT_PASSWORD";
constexpr char DASHBOARD_ID[] = "YOUR_DASHBOARD_ID";

IotNetESP32 tunnel;

const int potentiometerPin = 34;

void setup() {
  Serial.begin(115200);

  // Setup WiFi
  tunnel.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
  tunnel.setupMQTT(MQTT_USERNAME, MQTT_PASSWORD, DASHBOARD_ID);
  tunnel.addVirtualPin("V1", "Potentiometer");
  tunnel.addVirtualPin("V2", "LED");
  tunnel.setup();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(potentiometerPin, INPUT);
}

void loop() {
  tunnel.loop();

  // Read potentiometer and send to V2
  int sensorValue = analogRead(potentiometerPin);
  tunnel.virtualPinVisualization("V2", sensorValue);

  // Control LED based on V1
  int value = tunnel.virtualPinInteraction("V1");
  if (value == 1) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}
```

This example demonstrates how to use the IotNetESP32 library to connect to a WiFi network, setup MQTT communication, and control a virtual pin. The potentiometer is read and sent to a virtual pin, and the LED is controlled based on the value of the virtual pin.

For more detailed examples and documentation, please refer to the [examples](examples) folder in this repository.
