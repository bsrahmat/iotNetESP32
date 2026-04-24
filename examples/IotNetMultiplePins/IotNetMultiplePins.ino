#include <IotNetESP32.h>
#include <WiFi.h>

const int LED_PIN = 2;

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

void handleCounterAscending() {
    static unsigned long lastUpdate = 0;
    static int counter = 1;

    if (iotnet.shouldUpdate(lastUpdate, 50)) {
        iotnet.virtualWrite("V2", counter++);
        if (counter > 100) {
            counter = 1;
        }
    }
}

void handleCounterDescending() {
    static unsigned long lastUpdate = 0;
    static int counter = 100;

    if (iotnet.shouldUpdate(lastUpdate, 50)) {
        iotnet.virtualWrite("V3", counter--);
        if (counter < 1) {
            counter = 100;
        }
    }
}

void handlePinLED() {
    if (!iotnet.hasNewValue("V4")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V4");
    if(data == 1) {
        digitalWrite(LED_PIN, HIGH);
    } else if (data == 0) {
        digitalWrite(LED_PIN, LOW);
    }
}

void handlePinPotentiometer() {
    if (!iotnet.hasNewValue("V5")) {
        return;
    }

    int data = iotnet.virtualRead<int>("V5");
    iotnet.virtualWrite("V6", data);
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
    
    iotnet.begin(IOTNET_CONFIG);
}

void loop() {
    checkWiFiConnection();
    
    iotnet.run();
    
    handleVersion();
    handleCounterAscending();
    handleCounterDescending();
    handlePinLED();
    handlePinPotentiometer();
}
