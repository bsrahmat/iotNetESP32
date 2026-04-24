#include <IotNetESP32.h>
#include <WiFi.h>

// Define pin for potentiometer
const int POTENTIOMETER_PIN = 34;

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

IotNetESP32::ClientConfig IOTNET_CONFIG = {
    .mqttUsername = "DEVICE_ID",
    .mqttPassword = "DEVICE_SECRET",
    .boardIdentifier = "BOARD_ID",
    .firmwareVersion = "1.0.0",
    .enableOta = true
};

IotNetESP32 iotnet;

void handleVersion() {
    iotnet.virtualWrite("V1", iotnet.version());
}

void handlePotentiometer() {
    static unsigned long lastUpdate = 0;
    
    if (iotnet.shouldUpdate(lastUpdate, 100)) {
        int sensorValue = analogRead(POTENTIOMETER_PIN);
        iotnet.virtualWrite("V2", sensorValue);
    }
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
    
    pinMode(POTENTIOMETER_PIN, INPUT);

    setupWiFi();
    
    iotnet.begin(IOTNET_CONFIG);
}

void loop() {
    checkWiFiConnection();
    
    iotnet.run();
    
    handleVersion();
    handlePotentiometer();
}
