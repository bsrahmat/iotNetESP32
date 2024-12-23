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
