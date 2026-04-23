#include "IotNetESP32.h"

#include "core/TopicBuilder.h"

void IotNetESP32::updateBoardStatusInternal(const char *status) {
    if (!status || !credentials.mqttUsername || !credentials.boardName || !credentials.mqttPassword) {
        Serial.printf("Error: Missing parameters for updateBoardStatus (Current version: %s)\n",
                      currentFirmwareVersion);
        return;
    }

    if (strcmp(status, "pending") == 0) {
        startTimestamp = millis();
        timingActive = true;
    } else if ((strcmp(status, "success") == 0 || strcmp(status, "failed") == 0) && timingActive) {
        endTimestamp = millis();
        timingActive = false;
        String formattedTime = getFormattedExecutionTime();
        Serial.printf("Execution time: %s\n", formattedTime.c_str());
    }

    char topic[MAX_TOPIC_LENGTH];
    if (!iotnet::core::buildDeviceTopic(
            topic,
            sizeof(topic),
            credentials.mqttUsername,
            credentials.boardName,
            "status"
        )) {
        Serial.println("Failed to build board status topic");
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"status\":\"%s\"}",
             currentFirmwareVersion, status);

    if (!mqttClient.connected()) {
        Serial.printf("Cannot publish board status for version %s: MQTT not connected\n",
                      currentFirmwareVersion);
        return;
    }

    bool success = mqttClient.publish(topic, (const uint8_t *)payload, strlen(payload), false);
    if (!success) {
        Serial.printf("Failed to publish board status for version %s to topic: %s\n",
                      currentFirmwareVersion, topic);
    } else {
        Serial.printf("Update Status: %s (Version: %s)\n", status, currentFirmwareVersion);
    }
}

void IotNetESP32::registerBoardInternal() {
    if (!credentials.mqttUsername || !credentials.boardName || !credentials.mqttPassword) {
        Serial.println("Error: Missing parameters for registerBoard");
        return;
    }

    char topic[MAX_TOPIC_LENGTH];
    if (!iotnet::core::buildDeviceTopic(
            topic,
            sizeof(topic),
            credentials.mqttUsername,
            credentials.boardName,
            "board"
        )) {
        Serial.println("Failed to build board registration topic");
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"version\":\"%s\"}", currentFirmwareVersion);

    if (!mqttClient.connected()) {
        Serial.println("Cannot register board: MQTT not connected");

        if (reconnectMQTT()) {
            Serial.println("MQTT reconnected successfully in registerBoardInternal");
        } else {
            Serial.println("Failed to reconnect MQTT in registerBoardInternal");
            return;
        }
    }

    bool success = mqttClient.publish(topic, (const uint8_t *)payload, strlen(payload), false);
    if (!success) {
        Serial.printf("Failed to publish board registration to topic: %s\n", topic);
        delay(100);
        success = mqttClient.publish(topic, (const uint8_t *)payload, strlen(payload), false);
        if (!success) {
            Serial.println("Second attempt to publish board registration failed");
        }
    }
}

void IotNetESP32::publishBoardStatus(const char *status) {
    updateBoardStatusInternal(status);
}

void IotNetESP32::publishBoardRegistration() {
    registerBoardInternal();
}
