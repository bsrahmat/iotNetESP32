#ifndef IOTNETESP32_H
#define IOTNETESP32_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "i-ot.net.h"

class iotNetESP32 {
    private:
        static const int MAX_VIRTUAL_PINS = 20;
        struct DeviceState {
            String urlOta;
            String version;
            String dynamicTopics[MAX_VIRTUAL_PINS];
            String virtualPins[MAX_VIRTUAL_PINS];
            int virtualPinValues[MAX_VIRTUAL_PINS];
            int topicCount;
            bool updateRequired;

            DeviceState();
        };

        void setupSSL();
        void validateDevice();
        void updateStatusVersions(const char* status);
        void performFirmwareUpdate();
        void handleMQTTCallback(char* topic, byte* payload, unsigned int length);
        int updateVirtualPin(const char* pin, const char* pubValue, int storeValue);

        WiFiClientSecure mqttClient;
        WiFiClientSecure apiClient;
        HTTPClient apiHttps;
        WiFiClientSecure storageClient;
        HTTPClient storageHttps;
        PubSubClient mqttHandler;

    public:
        iotNetESP32();

        void setupWiFi(const char* ssid, const char* password);
        void setupMQTT(const char* username, const char* password, const char* dashboard);
        bool addVirtualPin(const char* pin, const char* pinName);
        int virtualPinInteraction(const char* pin);
        int virtualPinVisualization(const char* pin, int value);
        int virtualPinVisualization(const char* pin, float value);
        int virtualPinVisualization(const char* pin, const char* value);
        void setup();
        void loop();

        DeviceState state;
        portMUX_TYPE stateMux;
        const char* mqttUsername;
        const char* mqttPassword;
        const char* dashboardId;
        const char* signature;
        const char* nonce;

        friend void otaTask(void *pvParameters);
};

#endif
