#include "IotNetESP32.h"

// toString specializations
template <> const char *IotNetESP32::toString<float>(float value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%.2f", value);
    return buffer;
}

template <>
const char *IotNetESP32::toString<double>(double value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%.2f", value);
    return buffer;
}

template <> const char *IotNetESP32::toString<int>(int value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%d", value);
    return buffer;
}

template <>
const char *IotNetESP32::toString<unsigned int>(unsigned int value, char *buffer,
                                                size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%u", value);
    return buffer;
}

template <> const char *IotNetESP32::toString<long>(long value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%ld", value);
    return buffer;
}

template <>
const char *IotNetESP32::toString<unsigned long>(unsigned long value, char *buffer,
                                                 size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    snprintf(buffer, bufferSize, "%lu", value);
    return buffer;
}

template <>
const char *IotNetESP32::toString<const char *>(const char *value, char *buffer,
                                                size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    if (!value) {
        buffer[0] = '\0';
        return buffer;
    }
    strncpy(buffer, value, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

template <> const char *IotNetESP32::toString<bool>(bool value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    strncpy(buffer, value ? "true" : "false", bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

template <>
const char *IotNetESP32::toString<String>(String value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0)
        return "";
    strncpy(buffer, value.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

template <> int IotNetESP32::fromString<int>(const String &str) {
    return str.toInt();
}

template <> unsigned int IotNetESP32::fromString<unsigned int>(const String &str) {
    return (unsigned int)str.toInt();
}

template <> long IotNetESP32::fromString<long>(const String &str) {
    return str.toInt();
}

template <> unsigned long IotNetESP32::fromString<unsigned long>(const String &str) {
    return (unsigned long)str.toInt();
}

template <> float IotNetESP32::fromString<float>(const String &str) {
    return str.toFloat();
}

template <> double IotNetESP32::fromString<double>(const String &str) {
    return str.toDouble();
}

template <> bool IotNetESP32::fromString<bool>(const String &str) {
    return str.equalsIgnoreCase("true") || str == "1" || str.toInt() > 0;
}

template <> String IotNetESP32::fromString<String>(const String &str) {
    return str;
}

template <> char *IotNetESP32::fromString<char *>(const String &str) {
    return const_cast<char *>(str.c_str());
}

template <typename T> bool IotNetESP32::virtualWrite(const char *pin, T value) {
    return publishToPin(pin, value);
}

template <typename T> T IotNetESP32::virtualRead(const char *pin) {
    if (!pin) {
        return T();
    }

    int pinIndex = convertPinToIndex(pin);
    if (pinIndex < 0 || pinIndex >= MAX_PINS) {
        return T();
    }

    if (!pins[pinIndex].initialized) {
        initPinTopic(pinIndex);
        if (mqttClient.connected()) {
            mqttClient.subscribe(pins[pinIndex].topic);
        }
    }

    if (!pins[pinIndex].updated) {
        return T();
    }

    pins[pinIndex].updated = false;
    String value = String(pins[pinIndex].value);

    if (value.isEmpty()) {
        return T();
    }
    return fromString<T>(value);
}

template <typename T> bool IotNetESP32::publishToPin(const char *pin, T value) {
    if (!pin || !mqttClient.connected()) {
        return false;
    }

    int pinIndex = convertPinToIndex(pin);
    if (pinIndex < 0 || pinIndex >= MAX_PINS) {
        return false;
    }

    if (!pins[pinIndex].initialized) {
        initPinTopic(pinIndex);
    }

    char valueStr[32];
    toString(value, valueStr, sizeof(valueStr));

    if (pinIndex == 0) {
        return mqttClient.publish(pins[pinIndex].topic, (const uint8_t *)valueStr, strlen(valueStr),
                                  true);
    } else {
        return mqttClient.publish(pins[pinIndex].topic, (const uint8_t *)valueStr, strlen(valueStr),
                                  false);
    }
}

template <typename T> const char *IotNetESP32::toString(T value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return "";
    }
    snprintf(buffer, bufferSize, "%d", static_cast<int>(value));
    return buffer;
}

template <typename T> T IotNetESP32::fromString(const String &str) {
    return static_cast<T>(str.toInt());
}

template bool IotNetESP32::publishToPin<int>(const char *pin, int value);
template bool IotNetESP32::publishToPin<float>(const char *pin, float value);
template bool IotNetESP32::publishToPin<double>(const char *pin, double value);
template bool IotNetESP32::publishToPin<bool>(const char *pin, bool value);
template bool IotNetESP32::publishToPin<unsigned int>(const char *pin, unsigned int value);
template bool IotNetESP32::publishToPin<long>(const char *pin, long value);
template bool IotNetESP32::publishToPin<unsigned long>(const char *pin, unsigned long value);
template bool IotNetESP32::publishToPin<const char *>(const char *pin, const char *value);
template bool IotNetESP32::publishToPin<String>(const char *pin, String value);

template bool IotNetESP32::virtualWrite<int>(const char *pin, int value);
template bool IotNetESP32::virtualWrite<float>(const char *pin, float value);
template bool IotNetESP32::virtualWrite<double>(const char *pin, double value);
template bool IotNetESP32::virtualWrite<bool>(const char *pin, bool value);
template bool IotNetESP32::virtualWrite<unsigned int>(const char *pin, unsigned int value);
template bool IotNetESP32::virtualWrite<long>(const char *pin, long value);
template bool IotNetESP32::virtualWrite<unsigned long>(const char *pin, unsigned long value);
template bool IotNetESP32::virtualWrite<const char *>(const char *pin, const char *value);
template bool IotNetESP32::virtualWrite<String>(const char *pin, String value);

template int IotNetESP32::virtualRead<int>(const char *pin);
template float IotNetESP32::virtualRead<float>(const char *pin);
template double IotNetESP32::virtualRead<double>(const char *pin);
template bool IotNetESP32::virtualRead<bool>(const char *pin);
template unsigned int IotNetESP32::virtualRead<unsigned int>(const char *pin);
template long IotNetESP32::virtualRead<long>(const char *pin);
template unsigned long IotNetESP32::virtualRead<unsigned long>(const char *pin);
template String IotNetESP32::virtualRead<String>(const char *pin);
