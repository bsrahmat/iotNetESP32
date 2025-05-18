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
