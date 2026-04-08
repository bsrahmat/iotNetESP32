#include "IotNetESP32.h"
#include "i-ot.net.h"

static IotNetESP32 *currentInstance = nullptr;
extern const char *IOTNET_USERNAME;
extern const char *IOTNET_PASSWORD;
extern const char *IOTNET_BOARD_NAME;

//=======================================================================================
// Constructor
//=======================================================================================

IotNetESP32::IotNetESP32()
    : mqttClient(espClient), credentials{nullptr, nullptr, nullptr}, mqttConfig{nullptr, 0, 0},
      certificates{nullptr}, numCallbacks(0), startTimestamp(0), endTimestamp(0),
      timingActive(false), timeConfigured(false) {
    currentInstance = this;
    strcpy(currentFirmwareVersion, "1.0.0");
    strcpy(timeZone, "UTC");
    for (int i = 0; i < MAX_PINS; i++) {
        pins[i].initialized = false;
        pins[i].updated = false;
        pins[i].value[0] = '\0';
    }
}

//=======================================================================================
// Initialization and Connection Methods
//=======================================================================================

void IotNetESP32::begin() {
    this->credentials.mqttUsername = IOTNET_USERNAME;
    this->credentials.mqttPassword = IOTNET_PASSWORD;
    this->credentials.boardName = IOTNET_BOARD_NAME;

    initPinTopic(mqttConfig.statusPin);

    this->preferences.begin("iotnet", false);
    this->preferences.end();
    connect();
}

void IotNetESP32::connect() {
    setCertificates();
    setMQTTServer();
    setupCertificates();

    printLogo();

    unsigned long startAttemptTime = millis();
    bool connected = false;

    while (!connected && millis() - startAttemptTime < MQTT_TIMEOUT_MS) {
        connected = reconnectMQTT();
        if (!connected) {
            delay(RECONNECT_DELAY_MS);
        }
    }

    if (!connected) {
        Serial.println("Failed to connect to MQTT broker within timeout");
        return;
    }

    this->preferences.begin("iotnet", false);
    this->preferences.end();
}

void IotNetESP32::setCertificates() {
    this->certificates.caCert = var_6;
}

void IotNetESP32::setMQTTServer() {
    this->mqttConfig.server = var_2;
    this->mqttConfig.port = var_1;

    if (!mqttConfig.server || mqttConfig.port <= 0) {
        Serial.println("Error: Invalid MQTT server configuration");
        return;
    }

    mqttClient.setServer(mqttConfig.server, mqttConfig.port);
    mqttClient.setCallback(staticMqttCallback);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);
}

void IotNetESP32::setupCertificates() {
    if (!certificates.caCert) {
        Serial.println("Warning: CA certificate is not set");
        return;
    }

    if (certificates.caCert)
        espClient.setCACert(certificates.caCert);
}

void IotNetESP32::version(const char *version) {
    if (version) {
        strncpy(currentFirmwareVersion, version, sizeof(currentFirmwareVersion) - 1);
        currentFirmwareVersion[sizeof(currentFirmwareVersion) - 1] = '\0';
    }
}

const char *IotNetESP32::version() const {
    return currentFirmwareVersion;
}

void IotNetESP32::setStatusPin(int pin) {
    if (pin < 0 || pin >= MAX_PINS) {
        Serial.printf("Error: Invalid status pin index: %d\n", pin);
        return;
    }

    mqttConfig.statusPin = pin;
}

void IotNetESP32::printLogo() {
    Serial.println("");
    Serial.println("    ________  _______   ______________");
    Serial.println("   /  _/ __ \\/_  __/ | / / ____/_  __/");
    Serial.println("   / // / / / / / /  |/ / __/   / /");
    Serial.println(" _/ // /_/ / / / / /|  / /___  / /");
    Serial.println("/___/\\____/ /_/ /_/ |_/_____/ /_/");
    Serial.println("");
    Serial.printf("Firmware version: %s\n", currentFirmwareVersion);
}

//=======================================================================================
// Runtime and Operation Methods
//=======================================================================================

void IotNetESP32::run() {
    checkConnections();
    mqttClient.loop();

    for (int i = 0; i < numCallbacks; i++) {
        int pinIndex = callbacks[i].pinIndex;
        if (pinIndex < 0 || pinIndex >= MAX_PINS) {
            continue;
        }

        if (!pins[pinIndex].updated) {
            continue;
        }

        callbacks[i].callback(String(pins[pinIndex].value));
        pins[pinIndex].updated = false;
    }
}

void IotNetESP32::checkConnections() {
    if (!mqttClient.connected()) {
        Serial.println("MQTT connection lost. Reconnecting...");
        if (reconnectMQTT()) {
            Serial.println("MQTT reconnected successfully");
        } else {
            delay(RECONNECT_DELAY_MS);
        }
    }
}

bool IotNetESP32::reconnectMQTT() {
    if (mqttClient.connected()) {
        return true;
    }

    if (!credentials.mqttUsername || !credentials.mqttPassword || !credentials.boardName) {
        Serial.println("Error: MQTT credentials or board name not set");
        return false;
    }

    if (!pins[mqttConfig.statusPin].initialized) {
        initPinTopic(mqttConfig.statusPin);
    }

    bool connected = mqttClient.connect(credentials.boardName, credentials.mqttUsername,
                                        credentials.mqttPassword, pins[mqttConfig.statusPin].topic,
                                        1, true, "offline");

    if (!connected) {
        return false;
    }

    publishToPin("V0", "online");
    for (int i = 0; i < MAX_PINS; i++) {
        if (pins[i].initialized) {
            mqttClient.subscribe(pins[i].topic);
        }
    }
    return true;
}

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

//=======================================================================================
// Pin Management Methods
//=======================================================================================

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
        Serial.println(
            "Error: Cannot initialize pin topic - MQTT username or board name is not set");
        return;
    }

    snprintf(pins[pin].topic, sizeof(pins[pin].topic), "/device/%s/%s/V%d",
             credentials.mqttUsername, credentials.boardName, pin);

    // Initialize pin state
    pins[pin].initialized = true;
    pins[pin].value[0] = '\0';
    pins[pin].updated = false;
}

//=======================================================================================
// MQTT Communication Methods
//=======================================================================================

void IotNetESP32::staticMqttCallback(char *topic, byte *payload, unsigned int length) {
    if (currentInstance) {
        currentInstance->mqttCallback(topic, payload, length);
    }
}

void IotNetESP32::mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (!topic || !payload || length == 0) {
        return;
    }

    char *message = new char[length + 1];
    if (!message) {
        return;
    }

    memcpy(message, payload, length);
    message[length] = '\0';


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

    delete[] message;
}

void IotNetESP32::updateBoardStatus(const char *status) {
    if (!status || !credentials.mqttUsername || !credentials.boardName ||
        !credentials.mqttPassword) {
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

    char topic[120];
    snprintf(topic, sizeof(topic), "/device/%s/%s/status", credentials.mqttUsername,
             credentials.boardName);

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

void IotNetESP32::registerBoard() {
    if (!credentials.mqttUsername || !credentials.boardName || !credentials.mqttPassword) {
        Serial.println("Error: Missing parameters for registerBoard");
        return;
    }

    char topic[120];
    snprintf(topic, sizeof(topic), "/device/%s/%s/board", credentials.mqttUsername,
             credentials.boardName);

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"version\":\"%s\"}", currentFirmwareVersion);

    if (!mqttClient.connected()) {
        Serial.println("Cannot register board: MQTT not connected");

        if (reconnectMQTT()) {
            Serial.println("MQTT reconnected successfully in registerBoard");
        } else {
            Serial.println("Failed to reconnect MQTT in registerBoard");
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

unsigned long IotNetESP32::getExecutionTime() {
    if (endTimestamp > 0 && startTimestamp > 0) {
        return endTimestamp - startTimestamp;
    } else if (timingActive && startTimestamp > 0) {
        return millis() - startTimestamp;
    }
    return 0;
}

String IotNetESP32::getFormattedExecutionTime() {
    unsigned long totalTime = getExecutionTime();
    if (totalTime == 0) {
        return "0ms";
    }

    String formattedTime;

    unsigned long hours = totalTime / 3600000;
    unsigned long minutes = (totalTime % 3600000) / 60000;
    unsigned long seconds = (totalTime % 60000) / 1000;
    unsigned long milliseconds = totalTime % 1000;

    if (hours > 0) {
        formattedTime += String(hours) + "h ";
    }

    if (minutes > 0 || hours > 0) {
        formattedTime += String(minutes) + "m ";
    }

    if (seconds > 0 || minutes > 0 || hours > 0) {
        formattedTime += String(seconds) + "s ";
    }

    formattedTime += String(milliseconds) + "ms";

    return formattedTime;
}

//=======================================================================================
// Time Synchronization Methods
//=======================================================================================

void IotNetESP32::configureTime(const char *timezone, const char *ntpServer1,
                                const char *ntpServer2, const char *ntpServer3) {
    if (!timezone || !ntpServer1) {
        Serial.println("Error: Invalid time configuration parameters");
        return;
    }

    strncpy(timeZone, timezone, sizeof(timeZone) - 1);
    timeZone[sizeof(timeZone) - 1] = '\0';
    configTzTime(timezone, ntpServer1, ntpServer2, ntpServer3);

    int retry = 0;
    const int maxRetries = 10;

    Serial.print("Waiting for NTP time sync");
    while (!isTimeSet() && retry < maxRetries) {
        Serial.print(".");
        delay(500);
        retry++;
    }
    Serial.println("");

    if (isTimeSet()) {
        timeConfigured = true;
        Serial.println("Time synchronized with NTP server");
        Serial.printf("Current time: %s\n", getFormattedTime().c_str());
    } else {
        Serial.println("Failed to synchronize time with NTP server");
    }
}

bool IotNetESP32::isTimeSet() {
    time_t now = time(nullptr);
    return now > 1600000000;
}

String IotNetESP32::getFormattedTime(const char *format) {
    if (!isTimeSet()) {
        return "Time not synchronized";
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
    char buffer[80];

    localtime_r(&now, &timeinfo);
    strftime(buffer, sizeof(buffer), format, &timeinfo);

    return String(buffer);
}

//=======================================================================================
// Template Method Specializations and Implementations
//=======================================================================================

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

// Generic template implementations
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

// External template instance declarations
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