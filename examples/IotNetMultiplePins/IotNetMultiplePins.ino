#include <Arduino.h>
#include <IotNetESP32.h>
#include <WiFi.h>

const int LED_PIN = 2;

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

    setupWiFi();
    
    iotnet.version("1.0.0");
    iotnet.begin();
}

void loop() {
    checkWiFiConnection();
    
    iotnet.run();
    
    handlePinV1();
    handlePinV2();
    handlePinV3();
    handlePinV4();
}
