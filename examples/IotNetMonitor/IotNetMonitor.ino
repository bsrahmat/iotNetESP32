#include <Arduino.h>
#include <IotNetESP32.h>

// Define pin for potentiometer
int POTENTIOMETER_PIN = 34;

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Authentication credentials
const char* IOTNET_USERNAME = "YOUR_IOTNET_USERNAME";
const char* IOTNET_PASSWORD = "YOUR_IOTNET_PASSWORD";
const char* IOTNET_BOARD_NAME = "YOUR_IOTNET_BOARD_NAME";

IotNetESP32 iotnet;

void handlePotentiometer() {
    static unsigned long lastUpdate = 0;
    
    if (iotnet.shouldUpdate(lastUpdate, 100)) {
        int sensorValue = analogRead(POTENTIOMETER_PIN);
        iotnet.virtualWrite("V1", sensorValue);
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(POTENTIOMETER_PIN, INPUT);

    iotnet.version("1.0.0");
    iotnet.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    iotnet.run();
    
    handlePotentiometer();
}
