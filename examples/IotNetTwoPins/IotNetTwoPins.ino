#include <Arduino.h>
#include <IotNetESP32.h>
#include <WiFi.h>

// Define pins
const int LED_PIN = LED_BUILTIN;
const int POTENTIOMETER_PIN = 34;

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

void handleLED() {
    if (!iotnet.hasNewValue("V2")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V2");
    if(data == 1) {
        digitalWrite(LED_PIN, HIGH);
    } else if (data == 0) {
        digitalWrite(LED_PIN, LOW);
    }
}

void handleLoopback() {
    if (!iotnet.hasNewValue("V3")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V3");
    iotnet.virtualWrite("V4", data);
}

void setupWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        setupWiFi();
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(POTENTIOMETER_PIN, INPUT);

    setupWiFi();
    
    iotnet.version("1.0.0");
    iotnet.begin();
}

void loop() {
    checkWiFiConnection();
    
    iotnet.run();
    
    handlePotentiometer();
    handleLED();
    handleLoopback();
}
