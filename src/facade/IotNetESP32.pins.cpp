#include "IotNetESP32.h"

#include "core/TopicBuilder.h"

bool IotNetESP32::shouldUpdate(unsigned long &lastUpdate, unsigned long interval) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate < interval) {
        return false;
    }

    lastUpdate = currentMillis;
    return true;
}

bool IotNetESP32::hasNewValue(const char *pin) {
    if (!pin) {
        return false;
    }

    int pinIndex = convertPinToIndex(pin);
    if (pinIndex < 0 || pinIndex >= MAX_PINS) {
        return false;
    }

    if (!pins[pinIndex].initialized) {
        initPinTopic(pinIndex);
        if (mqttClient.connected()) {
            mqttClient.subscribe(pins[pinIndex].topic);
        }
    }

    return pins[pinIndex].updated;
}

void IotNetESP32::registerCallback(const char *pin, void (*callback)(String)) {
    if (!pin || !callback) {
        Serial.println("Error: Invalid callback registration parameters");
        return;
    }

    int pinIndex = convertPinToIndex(pin);
    if (pinIndex < 0 || pinIndex >= MAX_PINS || numCallbacks >= MAX_PINS) {
        return;
    }

    callbacks[numCallbacks].pinIndex = pinIndex;
    callbacks[numCallbacks].callback = callback;
    numCallbacks++;

    if (!pins[pinIndex].initialized) {
        initPinTopic(pinIndex);
        if (mqttClient.connected()) {
            mqttClient.subscribe(pins[pinIndex].topic);
        }
    }
}

int IotNetESP32::convertPinToIndex(const char *pin) {
    if (!pin || pin[0] != 'V') {
        return -1;
    }
    return atoi(pin + 1);
}

void IotNetESP32::initPinTopic(int pin) {
    if (pin < 0 || pin >= MAX_PINS || pins[pin].initialized) {
        return;
    }

    if (!credentials.mqttUsername || !credentials.boardName) {
        Serial.println("Error: Cannot initialize pin topic - MQTT username or board name is not set");
        return;
    }

    if (!iotnet::core::buildPinTopic(
            pins[pin].topic,
            sizeof(pins[pin].topic),
            credentials.mqttUsername,
            credentials.boardName,
            pin
        )) {
        Serial.println("Error: Failed to build pin topic");
        return;
    }

    pins[pin].initialized = true;
    pins[pin].value[0] = '\0';
    pins[pin].updated = false;
}

void IotNetESP32::mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (!topic || !payload || length == 0) {
        return;
    }

    char message[MAX_MESSAGE_BUFFER_SIZE];
    if (!copyPayloadToBuffer(payload, length, message, sizeof(message))) {
        Serial.printf("Warning: Dropping oversized MQTT message on topic %s (len=%u)\n", topic, length);
        return;
    }

    if (otaUpdatesEnabled && strlen(otaSessionResponseTopic) > 0 &&
        strcmp(topic, otaSessionResponseTopic) == 0) {
        handleOtaSessionResponse(message);
        return;
    }

    if (otaUpdatesEnabled && strlen(otaTopic) > 0 && strcmp(topic, otaTopic) == 0) {
        handleOtaMessage(message);
        return;
    }

    for (int i = 0; i < MAX_PINS; i++) {
        if (!pins[i].initialized) {
            continue;
        }

        if (strcmp(topic, pins[i].topic) != 0) {
            continue;
        }

        if (strcmp(pins[i].value, message) == 0) {
            break;
        }

        strncpy(pins[i].value, message, sizeof(pins[i].value) - 1);
        pins[i].value[sizeof(pins[i].value) - 1] = '\0';
        pins[i].updated = true;
        break;
    }
}

bool IotNetESP32::copyPayloadToBuffer(
    const byte *payload,
    unsigned int length,
    char *buffer,
    size_t bufferSize
) {
    if (!payload || !buffer || bufferSize == 0 || length >= bufferSize) {
        return false;
    }

    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    return true;
}
