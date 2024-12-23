#include <IotNetESP32.h>

// WiFi Credentials
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// MQTT Credentials
constexpr char MQTT_USERNAME[] = "YOUR_MQTT_USERNAME";
constexpr char MQTT_PASSWORD[] = "YOUR_MQTT_PASSWORD";
constexpr char DASHBOARD_ID[] = "YOUR_DASHBOARD_ID";

IotNetESP32 tunnel;

void setup() {
    Serial.begin(115200);
    tunnel.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    tunnel.setupMQTT(MQTT_USERNAME, MQTT_PASSWORD, DASHBOARD_ID);
    tunnel.setup();
}

void loop() {
    tunnel.loop();
}