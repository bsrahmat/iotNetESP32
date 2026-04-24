#include "IotNetESP32.h"
#include "i-ot.net.h"
#include "mqtt/MqttConnectionManager.h"
#include "core/ClientConfig.h"

static IotNetESP32 *currentInstance = nullptr;

//=======================================================================================
// Constructor
//=======================================================================================

IotNetESP32::IotNetESP32()
    : mqttClient(espClient), credentials{nullptr, nullptr, nullptr}, mqttConfig{nullptr, 0, 0},
      certificates{nullptr}, numCallbacks(0), startTimestamp(0), endTimestamp(0), timingActive(false),
      timeConfigured(false), otaUpdatesEnabled(false), otaInProgress(false) {
    currentInstance = this;
    strcpy(currentFirmwareVersion, "1.0.0");
    strcpy(timeZone, "UTC");
    otaTopic[0] = '\0';
    otaSessionRequestTopic[0] = '\0';
    otaSessionResponseTopic[0] = '\0';
    otaSession.reset();
    for (int i = 0; i < MAX_PINS; i++) {
        pins[i].initialized = false;
        pins[i].updated = false;
        pins[i].value[0] = '\0';
    }
    memset(callbacks, 0, sizeof(callbacks));

}

//=======================================================================================
// Initialization and Connection Methods
//=======================================================================================

// Legacy begin() removed: use begin(ClientConfig) instead.

void IotNetESP32::begin(const ClientConfig &config) {
    if (!applyRuntimeConfig(config)) {
        Serial.println("Error: Invalid runtime client config");
        return;
    }

    this->preferences.begin("iotnet", false);
    this->preferences.end();
    connect();
}

void IotNetESP32::begin(const char *mqttUsername,
                        const char *mqttPassword,
                        const char *boardIdentifier,
                        const char *firmwareVersion,
                        bool enableOta) {
    ClientConfig config = {
        .mqttUsername = mqttUsername,
        .mqttPassword = mqttPassword,
        .boardIdentifier = boardIdentifier,
        .firmwareVersion = firmwareVersion,
        .enableOta = enableOta
    };
    begin(config);
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
    this->certificates.caCert = var_4;
}

void IotNetESP32::setMQTTServer() {
    this->mqttConfig.server = var_2;
    this->mqttConfig.port = var_1;

    if (!iotnetesp32::mqtt::MqttConnectionManager::isServerConfigValid(
            mqttConfig.server,
            mqttConfig.port
        )) {
        Serial.println("Error: Invalid MQTT server configuration");
        return;
    }

    iotnetesp32::mqtt::MqttConnectionManager::configureTransport(
        mqttClient,
        mqttConfig.server,
        mqttConfig.port,
        60,
        30,
        static_cast<uint16_t>(MAX_MESSAGE_BUFFER_SIZE)
    );
    mqttClient.setCallback(staticMqttCallback);
}

bool IotNetESP32::applyRuntimeConfig(const ClientConfig &config) {
    if (!config.mqttUsername || !config.mqttPassword || !config.boardIdentifier) {
        return false;
    }

    strncpy(runtimeMqttUsername, config.mqttUsername, sizeof(runtimeMqttUsername) - 1);
    runtimeMqttUsername[sizeof(runtimeMqttUsername) - 1] = '\0';

    strncpy(runtimeMqttPassword, config.mqttPassword, sizeof(runtimeMqttPassword) - 1);
    runtimeMqttPassword[sizeof(runtimeMqttPassword) - 1] = '\0';

    strncpy(runtimeBoardName, config.boardIdentifier, sizeof(runtimeBoardName) - 1);
    runtimeBoardName[sizeof(runtimeBoardName) - 1] = '\0';

    this->credentials.mqttUsername = runtimeMqttUsername;
    this->credentials.mqttPassword = runtimeMqttPassword;
    this->credentials.boardIdentifier = runtimeBoardName;

    if (config.firmwareVersion) {
        version(config.firmwareVersion);
    }
    otaUpdatesEnabled = config.enableOta;

    return true;
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

    // Check for session key timeout (30 seconds)
    if (otaSession.isTimedOut(millis(), OTA_SESSION_TIMEOUT_MS)) {
        Serial.println("[OTA-SESSION] FAIL: Session key request timed out (30s)");
        otaSession.setWaiting(false);
        updateBoardStatusInternal("failed");
    }

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

    if (!credentials.mqttUsername || !credentials.mqttPassword || !credentials.boardIdentifier) {
        Serial.println("Error: MQTT credentials or board name not set");
        return false;
    }

    if (mqttConfig.statusPin < 0 || mqttConfig.statusPin >= MAX_PINS) {
        mqttConfig.statusPin = 0;
    }

    if (!pins[mqttConfig.statusPin].initialized) {
        initPinTopic(mqttConfig.statusPin);
    }

    bool connected = iotnetesp32::mqtt::MqttConnectionManager::connectWithLwt(
        mqttClient,
        credentials.boardIdentifier,
        credentials.mqttUsername,
        credentials.mqttPassword,
        pins[mqttConfig.statusPin].topic,
        "offline",
        1,
        true
    );

    if (!connected) {
        return false;
    }

    Serial.println("[MQTT] Connected");
    publishToPin("V0", "online");
    for (int i = 0; i < MAX_PINS; i++) {
        if (pins[i].initialized) {
            mqttClient.subscribe(pins[i].topic);
        }
    }

    // Auto-register board after successful MQTT connection
    registerBoardInternal();

    // Subscribe to OTA updates if enabled
    if (otaUpdatesEnabled) {
        subscribeToOtaUpdates();
    }

    return true;
}

//=======================================================================================
// MQTT Communication Methods
//=======================================================================================

void IotNetESP32::staticMqttCallback(char *topic, byte *payload, unsigned int length) {
    if (currentInstance) {
        currentInstance->mqttCallback(topic, payload, length);
    }
}
