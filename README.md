# i-ot.net Library

IotNetESP32 is a library for ESP32 that simplifies IoT development by providing pre-configured WiFi, MQTT, and HTTP client functionalities, enabling seamless integration with `https://i-ot.net` platforms.

## Features

- **WiFi Connectivity Management**: Efficiently manages WiFi connections to ensure the device stays connected to the network.
- **Secure MQTT Communication**: Supports encrypted MQTT communication for secure data transfer.
- **HTTP Client with SSL Support**: Provides HTTP client functionality with SSL support for secure interactions.
- **Virtual Pin System**: Enables device interaction through a virtual pin system, supporting up to 50 virtual pins.
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

### Basic Usage Example

```cpp
#include <Arduino.h>
#include <IotNetESP32.h>

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Authentication credentials
const char* IOTNET_USERNAME = "YOUR_IOTNET_USERNAME";
const char* IOTNET_PASSWORD = "YOUR_IOTNET_PASSWORD";
const char* IOTNET_BOARD_NAME = "YOUR_IOTNET_BOARD_NAME";

IotNetESP32 iotnet;

void setup() {
    Serial.begin(115200);

    iotnet.version("1.0.0");
    iotnet.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    iotnet.run();
}
```

### LED Control Example

Control an LED using a virtual pin:

```cpp
#include <Arduino.h>
#include <IotNetESP32.h>

int LED_PIN = LED_BUILTIN;

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Authentication credentials
const char* IOTNET_USERNAME = "YOUR_IOTNET_USERNAME";
const char* IOTNET_PASSWORD = "YOUR_IOTNET_PASSWORD";
const char* IOTNET_BOARD_NAME = "YOUR_IOTNET_BOARD_NAME";

IotNetESP32 iotnet;

void handlePinV1() {
    if (!iotnet.hasNewValue("V1")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V1");
    if(data == 1) {
        digitalWrite(LED_PIN, HIGH);
    } else if (data == 0) {
        digitalWrite(LED_PIN, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    iotnet.version("1.0.0");
    iotnet.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    iotnet.run();
    
    handlePinV1();
}
```

### Advanced Example: Multiple Virtual Pins

This example demonstrates how to use multiple virtual pins to create counters and control an LED:

```cpp
#include <Arduino.h>
#include <IotNetESP32.h>

int LED_PIN = 2;

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Authentication credentials
const char* IOTNET_USERNAME = "YOUR_IOTNET_USERNAME";
const char* IOTNET_PASSWORD = "YOUR_IOTNET_PASSWORD";
const char* IOTNET_BOARD_NAME = "YOUR_IOTNET_BOARD_NAME";

IotNetESP32 iotnet;

void handlePinV1() {
    static unsigned long lastUpdate = 0;
    static int counter = 1;

    if (iotnet.shouldUpdate(lastUpdate, 50)) {
        iotnet.virtualWrite("V1", counter++);
        if (counter > 100) {
            counter = 1;
        }
    }
}

void handlePinV2() {
    static unsigned long lastUpdate = 0;
    static int counter = 100;

    if (iotnet.shouldUpdate(lastUpdate, 50)) {
        iotnet.virtualWrite("V2", counter--);
        if (counter < 1) {
            counter = 100;
        }
    }
}

void handlePinV3() {
    if (!iotnet.hasNewValue("V3")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V3");
    if(data == 1) {
        digitalWrite(LED_PIN, HIGH);
    } else if (data == 0) {
        digitalWrite(LED_PIN, LOW);
    }
}

void handlePinV4() {
    if (!iotnet.hasNewValue("V4")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V4");
    iotnet.virtualWrite("V5", data);
}

void setup() {
    Serial.begin(115200);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    iotnet.version("1.0.0");
    iotnet.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    iotnet.run();
    
    handlePinV1();
    handlePinV2();
    handlePinV3();
    handlePinV4();
}
```

## Key Features and Functions

- `iotnet.begin(WIFI_SSID, WIFI_PASSWORD)`: Initialize WiFi connection with your credentials
- `iotnet.version(VERSION)`: Set the firmware version for OTA updates
- `iotnet.run()`: Main method to handle MQTT connection and message processing
- `iotnet.virtualRead<T>(PIN)`: Read data from a virtual pin with type conversion
- `iotnet.virtualWrite(PIN, VALUE)`: Write data to a virtual pin
- `iotnet.hasNewValue(PIN)`: Check if a virtual pin has a new value
- `iotnet.shouldUpdate(lastUpdate, interval)`: Helper for time-based updates

## Available Examples

For more detailed examples and documentation, please refer to the [examples](examples) folder in this repository:

- **IotNetBlank**: Basic template to start your project
- **IotNetControl**: Control an LED with virtual pins
- **IotNetCounter**: Implement counters and multiple pin interactions
- **IotNetMonitor**: Monitor sensors and display data
- **IotNetTwoPins**: Demonstrate interaction between two virtual pins
