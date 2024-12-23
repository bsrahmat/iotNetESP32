#include <iotNetESP32.h>

// WiFi Credentials
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// MQTT Credentials
constexpr char MQTT_USERNAME[] = "YOUR_MQTT_USERNAME";
constexpr char MQTT_PASSWORD[] = "YOUR_MQTT_PASSWORD";
constexpr char DASHBOARD_ID[] = "YOUR_DASHBOARD_ID";

iotNetESP32 tunnel;

const int potentiometerPin = 34;

void setup() {
  Serial.begin(115200);

  // Setup WiFi
  tunnel.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
  tunnel.setupMQTT(MQTT_USERNAME, MQTT_PASSWORD, DASHBOARD_ID);
  tunnel.addVirtualPin("V1", "Potentiometer");
  tunnel.setup();

  pinMode(potentiometerPin, INPUT);
}

void loop() {
  tunnel.loop();
  int value = analogRead(potentiometerPin);
  tunnel.virtualPinVisualization("V1", value);
}
