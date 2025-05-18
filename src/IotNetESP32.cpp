#include "IotNetESP32.h"
#include "i-ot.net.h"
#include <Preferences.h>

static IotNetESP32 *currentInstance = nullptr;
extern const char *CLIENT_CERT;
extern const char *CLIENT_KEY;
extern const char *IOTNET_USERNAME;
extern const char *IOTNET_PASSWORD;
extern const char *IOTNET_BOARD_NAME;
Preferences preferences;

//=======================================================================================
// Constructor
//=======================================================================================

IotNetESP32::IotNetESP32()
    : mqttClient(espClient), credentials{nullptr, nullptr, nullptr, nullptr, nullptr},
      mqttConfig{nullptr, 0, 0}, certificates{nullptr, nullptr, nullptr}, numCallbacks(0) {
    currentInstance = this;
    strcpy(currentFirmwareVersion, "1.0.0");
    for (int i = 0; i < MAX_PINS; i++) {
        pins[i].initialized = false;
        pins[i].updated = false;
        pins[i].value[0] = '\0';
    }
}

//=======================================================================================
// Initialization and Connection Methods
//=======================================================================================

void IotNetESP32::begin(const char *wifiSsid, const char *wifiPassword) {
    if (!wifiSsid || !wifiPassword) {
        Serial.println("Error: WiFi credentials cannot be null");
        return;
    }

    this->credentials.wifiSsid = wifiSsid;
    this->credentials.wifiPassword = wifiPassword;
    this->credentials.boardName = IOTNET_BOARD_NAME;

    initPinTopic(mqttConfig.statusPin);
    connect();
}

void IotNetESP32::connect() {
    setMQTTCredentials(IOTNET_USERNAME, IOTNET_PASSWORD);
    setCertificates();
    setMQTTServer();
    setupCertificates();
    setupWiFi(credentials.wifiSsid, credentials.wifiPassword);
    printLogo();

    bool justUpdated = false;
    preferences.begin("iotnet", false);
    justUpdated = preferences.getBool("justUpdated", false);
    if (justUpdated) {
        Serial.println("First boot after OTA update");
        preferences.putBool("justUpdated", false);
    }
    preferences.end();

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

    if (justUpdated) {
        publishToPin("V0", "online");
        Serial.println("Published online status after OTA update");
    }
}

void IotNetESP32::setMQTTCredentials(const char *username, const char *password) {
    if (!username || !password) {
        Serial.println("Error: MQTT credentials are not set");
        return;
    }
    this->credentials.mqttUsername = username;
    this->credentials.mqttPassword = password;
}

void IotNetESP32::setCertificates() {
    this->certificates.clientCert = CLIENT_CERT;
    this->certificates.clientKey = CLIENT_KEY;
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
    if (!certificates.caCert && !certificates.clientCert && !certificates.clientKey) {
        Serial.println("Warning: One or more certificates are not set");
        return;
    }

    if (certificates.caCert)
        espClient.setCACert(certificates.caCert);
    if (certificates.clientCert)
        espClient.setCertificate(certificates.clientCert);
    if (certificates.clientKey)
        espClient.setPrivateKey(certificates.clientKey);
}

void IotNetESP32::setupWiFi(const char *ssid, const char *password) {
    if (!ssid || !password) {
        Serial.println("Error: WiFi credentials cannot be null");
        return;
    }

    Serial.printf("\nConnecting to %s", ssid);
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    bool connected = false;

    while (!connected && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
        connected = (WiFi.status() == WL_CONNECTED);
        if (!connected) {
            delay(500);
            Serial.print(".");
        }
    }

    if (connected) {
        return;
    }

    Serial.println("\nFailed to connect to WiFi. Restarting...");
    delay(3000);
    ESP.restart();
}

void IotNetESP32::version(const char *version) {
    if (version) {
        strncpy(currentFirmwareVersion, version, sizeof(currentFirmwareVersion) - 1);
        currentFirmwareVersion[sizeof(currentFirmwareVersion) - 1] = '\0';
    }
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
    Serial.printf("WiFi connected\nIP: %s\n", WiFi.localIP().toString().c_str());
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
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        setupWiFi(credentials.wifiSsid, credentials.wifiPassword);
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi reconnected successfully");
            Serial.printf("Connected to %s with IP: %s\n", credentials.wifiSsid,
                          WiFi.localIP().toString().c_str());
        }
    }

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

    subscribeToOtaUpdates();
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

    if (credentials.mqttUsername && credentials.boardName) {
        char otaTopic[120];
        snprintf(otaTopic, sizeof(otaTopic), "/device/%s/%s/ota/update", credentials.mqttUsername,
                 credentials.boardName);

        if (strcmp(topic, otaTopic) == 0) {
            processOtaUpdate(message);
            delete[] message;
            return;
        }
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

    delete[] message;
}

void IotNetESP32::subscribeToOtaUpdates() {
    if (!mqttClient.connected() || !credentials.mqttUsername || !credentials.boardName) {
        Serial.println(
            "Cannot subscribe to OTA updates: MQTT not connected or credentials not set");
        return;
    }

    char otaTopic[120];
    snprintf(otaTopic, sizeof(otaTopic), "/device/%s/%s/ota/update", credentials.mqttUsername,
             credentials.boardName);

    bool success = mqttClient.subscribe(otaTopic);
    if (!success) {
        Serial.printf("Failed to subscribe to OTA update topic: %s\n", otaTopic);
    }
}

//=======================================================================================
// OTA Update Methods
//=======================================================================================

void IotNetESP32::processOtaUpdate(const char *otaMessage) {
    if (!otaMessage) {
        Serial.println("Error: Invalid OTA message");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, otaMessage);

    if (error) {
        Serial.printf("Failed to parse OTA update message: %s\n", error.c_str());
        return;
    }

    if (!doc["signature"].is<const char *>() || !doc["version"].is<const char *>() ||
        !doc["nonce"].is<uint32_t>()) {
        Serial.println("Invalid OTA update message format");
        return;
    }

    OtaUpdateInfo *otaInfo = new OtaUpdateInfo();
    if (!otaInfo) {
        Serial.println("Failed to allocate memory for OTA update info");
        return;
    }

    strncpy(otaInfo->signature, doc["signature"], sizeof(otaInfo->signature) - 1);
    otaInfo->signature[sizeof(otaInfo->signature) - 1] = '\0';

    strncpy(otaInfo->version, doc["version"], sizeof(otaInfo->version) - 1);
    otaInfo->version[sizeof(otaInfo->version) - 1] = '\0';

    strncpy(currentFirmwareVersion, doc["version"], sizeof(currentFirmwareVersion) - 1);
    currentFirmwareVersion[sizeof(currentFirmwareVersion) - 1] = '\0';

    otaInfo->nonce = doc["nonce"];
    registerBoard();
    updateBoardStatus("pending");

    xTaskCreate(otaUpdateTask, "OTA_UPDATE_TASK", 32768, otaInfo, 1, NULL);
}

void IotNetESP32::otaUpdateTask(void *parameters) {
    OtaUpdateInfo *otaInfo = static_cast<OtaUpdateInfo *>(parameters);

    if (!otaInfo) {
        Serial.println("Error: Invalid OTA update parameters");
        vTaskDelete(NULL);
        return;
    }

    String otaUrl;
    HTTPClient http;
    WiFiClientSecure client;

    client.setCACert(var_4);
    client.setHandshakeTimeout(120);

    char url[256];
    snprintf(url, sizeof(url), "%s/keys/%s/ota", var_3, IOTNET_USERNAME);

    char requestBody[256];
    snprintf(requestBody, sizeof(requestBody),
             "{\"version\":\"%s\",\"signature\":\"%s\",\"nonce\":%u}", otaInfo->version,
             otaInfo->signature, otaInfo->nonce);

    if (!http.begin(client, url)) {
        Serial.println("Failed to begin HTTP request");
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    char authHeader[100];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", IOTNET_PASSWORD);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authHeader);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode <= 0) {
        Serial.printf("Error on HTTP request, code: %d\n", httpResponseCode);
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        http.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    JsonDocument responseDoc;
    DeserializationError responseError = deserializeJson(responseDoc, http.getString());

    bool isSuccess = !responseError && responseDoc["status"] == "success" &&
                     responseDoc["data"]["ota_url"].is<const char *>();

    if (!isSuccess) {
        if (!responseError && responseDoc["message"].is<const char *>()) {
            Serial.println(responseDoc["message"].as<String>());
        } else {
            Serial.println("Invalid response format");
        }

        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        http.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    otaUrl = responseDoc["data"]["ota_url"].as<String>();
    http.end();

    Serial.println("Starting OTA firmware update...");
    if (currentInstance)
        currentInstance->updateBoardStatus("active");

    HTTPClient updateHttp;
    WiFiClientSecure updateClient;

    updateClient.setCACert(var_4);
    updateClient.setHandshakeTimeout(120);

    esp_task_wdt_reset();

    if (!updateHttp.begin(updateClient, otaUrl)) {
        Serial.println("Failed to begin HTTP update request");
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    updateHttp.setTimeout(60000);

    int httpCode = updateHttp.GET();
    if (httpCode <= 0) {
        Serial.printf("HTTP GET failed, error: %s\n", updateHttp.errorToString(httpCode).c_str());
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        updateHttp.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET returned code %d\n", httpCode);
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        updateHttp.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    int contentLength = updateHttp.getSize();
    if (contentLength <= 0) {
        Serial.println("Error: Invalid content length for firmware file");
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        updateHttp.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (!Update.begin(contentLength, U_FLASH, -1, LOW)) {
        Serial.println("Not enough space to begin OTA update");
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        updateHttp.end();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    uint8_t buff[512] = {0};
    WiFiClient *stream = updateHttp.getStreamPtr();
    size_t written = 0;

    Serial.println("Downloading and flashing firmware...");

    const unsigned long timeout = 10000;
    unsigned long lastData = millis();
    unsigned long lastAnimation = 0;
    int lastProgressPercent = 0;
    const char animationChars[] = {'|', '/', '-', '\\'};
    uint8_t animIdx = 0;
    const int progressBarWidth = 50;

    while (updateHttp.connected() && (written < contentLength)) {
        size_t available = stream->available();

        if (available) {
            lastData = millis();
            size_t readBytes = stream->readBytes(buff, min(available, sizeof(buff)));

            if (readBytes > 0) {
                if (Update.write(buff, readBytes) != readBytes) {
                    Serial.println("Error during update write");
                    if (currentInstance)
                        currentInstance->updateBoardStatus("failed");
                    updateHttp.end();
                    Update.abort();
                    delete otaInfo;

                    memset(buff, 0, sizeof(buff));

                    vTaskDelete(NULL);
                    return;
                }

                written += readBytes;

                if (millis() - lastAnimation >= 200) {
                    int progressPercent = (written * 100) / contentLength;
                    int progressChars = (written * progressBarWidth) / contentLength;

                    if (progressPercent != lastProgressPercent) {
                        Serial.print("[");
                        for (int i = 0; i < progressBarWidth; i++) {
                            if (i < progressChars) {
                                Serial.print("=");
                            } else if (i == progressChars) {
                                Serial.print(">");
                            } else {
                                Serial.print(" ");
                            }
                        }
                        Serial.printf("] %3d%% %c\r", progressPercent, animationChars[animIdx]);

                        lastProgressPercent = progressPercent;
                        animIdx = (animIdx + 1) % 4;
                        lastAnimation = millis();
                    }

                    if (written % 10240 == 0) {
                        esp_task_wdt_reset();
                    }
                }
            }
        } else {
            if (millis() - lastData > timeout) {
                Serial.println("\nData transfer timeout");
                break;
            }
            delay(10);
        }

        yield();
    }

    updateHttp.end();
    memset(buff, 0, sizeof(buff));

    if (written != contentLength) {
        Serial.println("\nError: Downloaded size doesn't match expected size");
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        Update.abort();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    if (!Update.end(true)) {
        Serial.printf("\nError finalizing update: %s\n", Update.errorString());
        if (currentInstance)
            currentInstance->updateBoardStatus("failed");
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    stream = nullptr;

    Serial.println("Freeing heap memory...");
    uint32_t beforeClean = ESP.getFreeHeap();
    for (int i = 0; i < 10; i++) {
        ESP.getFreeHeap();
    }
    uint32_t afterClean = ESP.getFreeHeap();
    Serial.printf("Freed %d bytes of memory\n", afterClean - beforeClean);
    heap_caps_check_integrity_all(true);

    WiFi.getSleep();
    updateClient.stop();
    updateClient.flush();
    delay(100);

    if (currentInstance)
        currentInstance->updateBoardStatus("success");
    Serial.println("\nFirmware download completed");
    Serial.println("OTA update completed successfully! Rebooting...");

    preferences.begin("iotnet", false);
    preferences.putBool("justUpdated", true);
    preferences.end();

    delay(1000);
    ESP.restart();
}

void IotNetESP32::updateBoardStatus(const char *status) {
    if (!status || !credentials.mqttUsername || !credentials.boardName ||
        !credentials.mqttPassword) {
        Serial.println("Error: Missing parameters for updateBoardStatus");
        return;
    }

    HTTPClient http;
    WiFiClientSecure client;

    client.setCACert(var_4);

    char url[256];
    snprintf(url, sizeof(url), "%s/keys/%s/board", var_3, IOTNET_USERNAME);

    char requestBody[512];
    snprintf(requestBody, sizeof(requestBody),
             "{\"version\":\"%s\",\"board_identifier\":\"%s\",\"status\":\"%s\"}",
             currentFirmwareVersion, IOTNET_BOARD_NAME, status);

    if (!http.begin(client, url)) {
        Serial.println("Failed to begin HTTP request");
        return;
    }

    char authHeader[100];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", IOTNET_PASSWORD);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authHeader);

    int httpResponseCode = http.PATCH(requestBody);

    if (httpResponseCode != HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && doc["message"].is<const char *>()) {
            Serial.println(doc["message"].as<String>());
        } else {
            Serial.printf("Error on HTTP request, code: %d\n", httpResponseCode);
        }
    }
    http.end();
}

void IotNetESP32::registerBoard() {
    if (!credentials.mqttUsername || !credentials.boardName || !credentials.mqttPassword) {
        Serial.println("Error: Missing parameters for registerBoard");
        return;
    }

    HTTPClient http;
    WiFiClientSecure client;

    client.setCACert(var_4);

    char url[256];
    snprintf(url, sizeof(url), "%s/keys/%s/board", var_3, IOTNET_USERNAME);

    char requestBody[512];
    snprintf(requestBody, sizeof(requestBody), "{\"version\":\"%s\",\"board_identifier\":\"%s\"}",
             currentFirmwareVersion, IOTNET_BOARD_NAME);

    if (!http.begin(client, url)) {
        Serial.println("Failed to begin HTTP request");
        return;
    }

    char authHeader[100];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", IOTNET_PASSWORD);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authHeader);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode != HTTP_CODE_CREATED) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && doc["message"].is<const char *>()) {
            Serial.println(doc["message"].as<String>());
        } else {
            Serial.printf("Error on HTTP request, code: %d\n", httpResponseCode);
        }
    }
    http.end();
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

// fromString specializations
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

    return mqttClient.publish(pins[pinIndex].topic, valueStr, true);
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
