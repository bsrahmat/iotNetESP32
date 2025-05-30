#include "IotNetESP32.h"
#include "i-ot.net.h"

static IotNetESP32 *currentInstance = nullptr;
extern const char *CLIENT_CERT;
extern const char *CLIENT_KEY;
extern const char *IOTNET_USERNAME;
extern const char *IOTNET_PASSWORD;
extern const char *IOTNET_BOARD_NAME;

//=======================================================================================
// Constructor
//=======================================================================================

IotNetESP32::IotNetESP32()
    : mqttClient(espClient), credentials{nullptr, nullptr, nullptr}, mqttConfig{nullptr, 0, 0},
      certificates{nullptr, nullptr, nullptr}, numCallbacks(0), startTimestamp(0),
      endTimestamp(0), timingActive(false), timeConfigured(false),
      initialHeapSize(0), minHeapDuringOta(0), minStackRemaining(0),
      maxCpuUsage(0), lastCpuCheckTime(0), totalCpuTimeUsed(0) {
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
    bool otaInProgress = this->preferences.getBool("otaInProgress", false);
    bool justUpdated = this->preferences.getBool("justUpdated", false);
    bool pubFailed = this->preferences.getBool("pubFailed", false);
    
    if (otaInProgress && !justUpdated) {
        Serial.println("Detected interrupted OTA update");
        String targetVersion = this->preferences.getString("targetVer", "");
        if (targetVersion.length() > 0) {
            Serial.printf("Found target version %s for failure reporting (current installed: %s)\n",
                          targetVersion.c_str(), currentFirmwareVersion);
        } else {
            Serial.println("Warning: No target version found for interrupted OTA update");
        }
        this->preferences.putBool("otaInProgress", false);
        this->preferences.putBool("pubFailed", true); 
    } 

    if (justUpdated) {
        this->preferences.putBool("pubFailed", false);
        this->preferences.putBool("otaInProgress", false);
        
        String newVersion = this->preferences.getString("targetVer", "");
        if (newVersion.length() > 0) {
            strncpy(currentFirmwareVersion, newVersion.c_str(), sizeof(currentFirmwareVersion) - 1);
            currentFirmwareVersion[sizeof(currentFirmwareVersion) - 1] = '\0';
        }
    }

    this->preferences.end();
    connect();
}

void IotNetESP32::connect() {
    setCertificates();
    setMQTTServer();
    setupCertificates();

    bool justUpdated = false;
    bool otaInterrupted = false;

    this->preferences.begin("iotnet", false);
    justUpdated = this->preferences.getBool("justUpdated", false);
    otaInterrupted = this->preferences.getBool("otaInProgress", false);
    bool pubFailed = this->preferences.getBool("pubFailed", false);
    
    if (justUpdated) {
        String newVersion = this->preferences.getString("targetVer", "");
        if (newVersion.length() > 0) {
            strncpy(currentFirmwareVersion, newVersion.c_str(), sizeof(currentFirmwareVersion) - 1);
            currentFirmwareVersion[sizeof(currentFirmwareVersion) - 1] = '\0';
        }
    }

    if (otaInterrupted) {
        this->preferences.putBool("otaInProgress", false);
    }
    
    printLogo();
    this->preferences.end();

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
    bool needPublishFailed = this->preferences.getBool("pubFailed", false);

    if (justUpdated) {
        this->preferences.putBool("pubFailed", false);
        this->preferences.putBool("justUpdated", false);
        updateBoardStatus("success");
        publishToPin("V0", "online");
    }
    else if (needPublishFailed) {
        String targetVersion = this->preferences.getString("targetVer", "");
        if (targetVersion.length() > 0) {
            Serial.printf("Reporting previous OTA update failure for version %s\n", targetVersion.c_str());
        } else {
            Serial.println("Reporting previous OTA update failure (version unknown)");
        }
        
        updateBoardStatus("failed");
        this->preferences.putBool("pubFailed", false);
    }
    
    this->preferences.end();

    if (otaInterrupted && !justUpdated) {
        updateBoardStatus("failed");
        Serial.println("Reported interrupted OTA update as failed");
    }
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

    this->preferences.begin("iotnet", false);
    this->preferences.putBool("otaInProgress", true);
    this->preferences.putString("targetVer", String(doc["version"].as<const char *>()));
    this->preferences.end();

    otaInfo->nonce = doc["nonce"];
    registerBoard();
    updateBoardStatus("pending");
    otaUpdateTask(otaInfo);
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
        if (currentInstance) {
            Serial.printf("OTA update failed for version %s: Failed to begin HTTP request\n", otaInfo->version);
            currentInstance->updateBoardStatus("failed");
        }
        client.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
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
        if (currentInstance) {
            Serial.printf("OTA update failed for version %s: HTTP error code %d\n", otaInfo->version, httpResponseCode);
            currentInstance->updateBoardStatus("failed");
        }
        http.end();
        client.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
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
        client.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
        vTaskDelete(NULL);
        return;
    }

    otaUrl = responseDoc["data"]["ota_url"].as<String>();
    http.end();
    client.stop();

    Serial.println("Starting OTA firmware update...");
    if (currentInstance) {
        currentInstance->updateBoardStatus("active");
        currentInstance->startResourceMonitoring();
    }

    Serial.print(F("Free memory before OTA: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    Serial.println(F(""));

    HTTPClient updateHttp;
    WiFiClientSecure updateClient;

    updateClient.setCACert(var_4);
    updateClient.setHandshakeTimeout(120);

    if (!updateHttp.begin(updateClient, otaUrl)) {
        Serial.println("Failed to begin HTTP update request");
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        updateClient.stop();
        delete otaInfo;
        vTaskDelete(NULL);
        return;
    }

    updateHttp.setTimeout(60000);

    int httpCode = updateHttp.GET();
    if (httpCode <= 0) {
        Serial.printf("HTTP GET failed, error: %s\n", updateHttp.errorToString(httpCode).c_str());
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        updateHttp.end();
        updateClient.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
        vTaskDelete(NULL);
        return;
    }

    int contentLength = updateHttp.getSize();
    if (contentLength <= 0) {
        Serial.println("Error: Invalid content length for firmware file");
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        updateHttp.end();
        updateClient.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (!Update.begin(contentLength, U_FLASH, -1, LOW)) {
        Serial.println("Not enough space to begin OTA update");
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        updateHttp.end();
        updateClient.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
        vTaskDelete(NULL);
        return;
    }

    uint8_t buff[512] = {0};
    WiFiClient *stream = updateHttp.getStreamPtr();
    size_t written = 0;

    Serial.println("Downloading and flashing firmware...");

    const unsigned long timeout = 10000;
    unsigned long lastData = millis();
    unsigned long lastProgressUpdate = 0;
    int lastProgressPercent = 0;

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
                    updateClient.stop();
                    Update.abort();
                    delete otaInfo;

                    memset(buff, 0, sizeof(buff));
                    delay(1000);
                    ESP.restart();

                    vTaskDelete(NULL);
                    return;
                }

                written += readBytes;

                if (millis() - lastProgressUpdate >= 1000) {
                    int progressPercent = (written * 100) / contentLength;

                    if (progressPercent != lastProgressPercent) {
                        Serial.printf("OTA Progress: %d%%\n", progressPercent);
                        lastProgressPercent = progressPercent;
                        lastProgressUpdate = millis();
                        
                        // Update resource monitoring during download
                        if (currentInstance) {
                            currentInstance->updateResourceMonitoring();
                        }
                    }
                }
            }
        } else {
            if (millis() - lastData > timeout) {
                Serial.println("Data transfer timeout");
                
                if (currentInstance) {
                    currentInstance->updateBoardStatus("failed");
                    currentInstance->preferences.begin("iotnet", false);
                    currentInstance->preferences.putBool("otaInProgress", false);
                    currentInstance->preferences.end();
                }
                
                Update.abort();
                updateClient.stop();
                delete otaInfo;
                memset(buff, 0, sizeof(buff));
                delay(1000);
                ESP.restart();
                
                vTaskDelete(NULL);
                return;
            }
            delay(10);
        }

        yield();
    }

    updateHttp.end();
    memset(buff, 0, sizeof(buff));

    if (written != contentLength) {
        Serial.println("\nError: Downloaded size doesn't match expected size");
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        Update.abort();
        updateClient.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
        vTaskDelete(NULL);
        return;
    }

    if (!Update.end(true)) {
        Serial.printf("\nError finalizing update: %s\n", Update.errorString());
        if (currentInstance) {
            currentInstance->updateBoardStatus("failed");
            currentInstance->preferences.begin("iotnet", false);
            currentInstance->preferences.putBool("otaInProgress", false);
            currentInstance->preferences.end();
        }
        updateClient.stop();
        delete otaInfo;
        
        delay(1000);
        ESP.restart();
        
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
    
    // Final resource usage report
    if (currentInstance) {
        currentInstance->stopResourceMonitoring();
    }

    updateClient.stop();
    updateClient.flush();
    delay(100);

    Serial.println("\nFirmware download completed");
    Serial.println("OTA update completed successfully! Rebooting...");

    currentInstance->preferences.begin("iotnet", false);
    currentInstance->preferences.putBool("justUpdated", true);
    currentInstance->preferences.putBool("otaInProgress", false);
    currentInstance->preferences.putBool("pubFailed", false);
    currentInstance->preferences.end();

    delete otaInfo; 

    delay(1000);
    ESP.restart();
}

//=======================================================================================
// Utilities Methods
//=======================================================================================

size_t IotNetESP32::getFreeHeap() {
    return ESP.getFreeHeap();
}

void IotNetESP32::startResourceMonitoring() {
    // Record initial heap size
    initialHeapSize = ESP.getFreeHeap();
    minHeapDuringOta = initialHeapSize;
    
    // Initialize stack monitoring
    minStackRemaining = uxTaskGetStackHighWaterMark(NULL);
    
    // Initialize CPU monitoring
    maxCpuUsage = 0.0;
    totalCpuTimeUsed = 0;
    lastCpuCheckTime = millis();
    
    // Ensure time tracking is enabled if not already set by updateBoardStatus("pending")
    if (!timingActive) {
        startTimestamp = millis();
        timingActive = true;
        Serial.println("Starting execution time measurement");
    }
    
    Serial.printf("Resource monitoring started - Initial heap: %u bytes, Initial stack remaining: %u\n", 
                  initialHeapSize, minStackRemaining);
}

void IotNetESP32::updateResourceMonitoring() {
    // Update heap monitoring
    size_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < minHeapDuringOta) {
        minHeapDuringOta = currentHeap;
    }
    
    // Update stack monitoring
    UBaseType_t currentStackRemaining = uxTaskGetStackHighWaterMark(NULL);
    if (currentStackRemaining < minStackRemaining) {
        minStackRemaining = currentStackRemaining;
    }
    
    // Update CPU monitoring (simplified approach)
    float currentCpuUsage = getCpuUsage();
    if (currentCpuUsage > maxCpuUsage) {
        maxCpuUsage = currentCpuUsage;
    }
    
    // Periodic reporting during update
    static unsigned long lastReportTime = 0;
    if (millis() - lastReportTime > 5000) {  // Report every 5 seconds
        Serial.printf("Resource usage - Heap: %u bytes (%.1f%% used), Stack remaining: %u, CPU: %.1f%%\n",
                     currentHeap,
                     100.0 * (1.0 - ((float)currentHeap / initialHeapSize)),
                     currentStackRemaining,
                     currentCpuUsage);
        lastReportTime = millis();
    }
}

void IotNetESP32::stopResourceMonitoring() {
    // If timing is still active, record the end timestamp
    if (timingActive && startTimestamp > 0) {
        endTimestamp = millis();
        timingActive = false;
        Serial.println("Stopping execution time measurement");
    }
    
    reportResourceUsage();
}

void IotNetESP32::reportResourceUsage() {
    size_t finalHeap = ESP.getFreeHeap();
    size_t heapUsed = (initialHeapSize > finalHeap) ? (initialHeapSize - finalHeap) : 0;
    float heapUsagePercent = (initialHeapSize > 0) ? (100.0 * heapUsed / initialHeapSize) : 0.0;
    
    Serial.println("\n--- OTA Update Resource Usage Report ---");
    Serial.printf("Heap usage: Initial %u bytes, Minimum %u bytes, Final %u bytes\n", 
                  initialHeapSize, minHeapDuringOta, finalHeap);
    Serial.printf("Maximum heap usage: %u bytes (%.1f%%)\n", 
                  heapUsed, heapUsagePercent);
    Serial.printf("Minimum stack remaining: %u bytes\n", minStackRemaining);
    Serial.printf("Maximum CPU usage: %.1f%%\n", maxCpuUsage);
    
    // Calculate and include total execution time in the report
    unsigned long totalTime = 0;
    String executionTimeStr;
    
    if (endTimestamp > 0 && startTimestamp > 0) {
        totalTime = endTimestamp - startTimestamp;
        executionTimeStr = getFormattedExecutionTime();
    } else if (timingActive && startTimestamp > 0) {
        totalTime = millis() - startTimestamp;
        
        // Format the time manually since getFormattedExecutionTime() might not work during active timing
        unsigned long hours = totalTime / 3600000;
        unsigned long minutes = (totalTime % 3600000) / 60000;
        unsigned long seconds = (totalTime % 60000) / 1000;
        unsigned long milliseconds = totalTime % 1000;
        
        executionTimeStr = "";
        if (hours > 0) {
            executionTimeStr += String(hours) + "h ";
        }
        if (minutes > 0 || hours > 0) {
            executionTimeStr += String(minutes) + "m ";
        }
        if (seconds > 0 || minutes > 0 || hours > 0) {
            executionTimeStr += String(seconds) + "s ";
        }
        executionTimeStr += String(milliseconds) + "ms";
    }
    
    if (totalTime > 0) {
        Serial.printf("Total update execution time: %s (from 'pending' to now)\n", executionTimeStr.c_str());
        
        // Calculate and display average data rates
        if (initialHeapSize > 0) {
            float bytesPerSec = (float)heapUsed / (totalTime / 1000.0f);
            Serial.printf("Average memory allocation rate: %.2f bytes/sec\n", bytesPerSec);
        }
    }
    
    Serial.println("-------------------------------------");
}

UBaseType_t IotNetESP32::getCurrentTaskStackRemaining() {
    return uxTaskGetStackHighWaterMark(NULL);
}

float IotNetESP32::getCpuUsage() {
    // Improved CPU usage calculation for ESP32
    static unsigned long lastTotalTime = 0;
    static unsigned long lastIdleTime = 0;
    
    unsigned long now = millis();
    unsigned long timeDelta = now - lastCpuCheckTime;
    
    if (timeDelta < 100) { // Only sample every 100ms for stability
        return maxCpuUsage;
    }
    
    // Get current total and idle times
    unsigned long totalTime = esp_timer_get_time() / 1000; // convert to ms
    unsigned long idleTime = 0;
    
    #ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
        // If FreeRTOS runtime stats are enabled, we can get idle task time
        idleTime = ulTaskGetRunTimeCounter(xTaskGetIdleTaskHandle()) / 1000; // convert to ms
    #else
        // Fallback method: estimate idle time from memory allocations and operations
        // This is not accurate but gives some indication
        idleTime = (ESP.getMaxAllocHeap() > 10000) ? totalTime / 4 : totalTime / 10;
    #endif
    
    // Calculate CPU usage
    float usage = 0.0;
    if (lastTotalTime > 0 && totalTime > lastTotalTime) {
        unsigned long totalDelta = totalTime - lastTotalTime;
        unsigned long idleDelta = (idleTime > lastIdleTime) ? (idleTime - lastIdleTime) : 0;
        
        // Calculate percentage of non-idle time
        if (totalDelta > 0) {
            usage = 100.0f * (1.0f - (float)idleDelta / (float)totalDelta);
        }
    }
    
    // Update last measurements
    lastTotalTime = totalTime;
    lastIdleTime = idleTime;
    lastCpuCheckTime = now;
    
    // Bound to reasonable values (0-100%)
    if (usage < 0.0f) usage = 0.0f;
    if (usage > 100.0f) usage = 100.0f;
    
    return usage;
}

void IotNetESP32::updateBoardStatus(const char *status) {
    if (!status || !credentials.mqttUsername || !credentials.boardName ||
        !credentials.mqttPassword) {
        Serial.printf("Error: Missing parameters for updateBoardStatus (Current version: %s)\n", currentFirmwareVersion);
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
    
    if (strcmp(status, "failed") == 0 || strcmp(status, "pending") == 0 || 
        strcmp(status, "active") == 0 || strcmp(status, "success") == 0) {
        this->preferences.begin("iotnet", false);
        String targetVersion = this->preferences.getString("targetVer", "");
        this->preferences.end();
        
        if (targetVersion.length() > 0) {
            snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"status\":\"%s\"}",
                    targetVersion.c_str(), status);
        } else {
            snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"status\":\"%s\"}",
                    currentFirmwareVersion, status);
        }
    } else {
        snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"status\":\"%s\"}",
                currentFirmwareVersion, status);
    }

    if (!mqttClient.connected()) {
        const char* versionToLog;
        if (strcmp(status, "failed") == 0 || strcmp(status, "pending") == 0 || strcmp(status, "active") == 0) {
            this->preferences.begin("iotnet", false);
            String targetVersion = this->preferences.getString("targetVer", "");
            this->preferences.end();
            
            if (targetVersion.length() > 0) {
                versionToLog = targetVersion.c_str();
            } else {
                versionToLog = currentFirmwareVersion;
            }
        } else {
            versionToLog = currentFirmwareVersion;
        }
        
        Serial.printf("Cannot publish board status for version %s: MQTT not connected\n", versionToLog);
        
        if (strcmp(status, "failed") == 0) {
            this->preferences.begin("iotnet", false);
            this->preferences.putBool("pubFailed", true);
            this->preferences.end();
            Serial.printf("Saved failed status for version %s to preferences for next boot\n", versionToLog);
        }
        return;
    }

    bool success = mqttClient.publish(topic, (const uint8_t*)payload, strlen(payload), false); 
    if (!success) {
        const char* versionToLog;
        if (strcmp(status, "failed") == 0 || strcmp(status, "pending") == 0 || strcmp(status, "active") == 0) {
            this->preferences.begin("iotnet", false);
            String targetVersion = this->preferences.getString("targetVer", "");
            this->preferences.end();
            
            if (targetVersion.length() > 0) {
                versionToLog = targetVersion.c_str();
            } else {
                versionToLog = currentFirmwareVersion;
            }
        } else {
            versionToLog = currentFirmwareVersion;
        }
        
        Serial.printf("Failed to publish board status for version %s to topic: %s\n", versionToLog, topic);
        
        if (strcmp(status, "failed") == 0) {
            this->preferences.begin("iotnet", false);
            this->preferences.putBool("pubFailed", true);
            this->preferences.putBool("justUpdated", false); // Make sure justUpdated is false for a failed update
            this->preferences.end();
            Serial.printf("Saved failed status for version %s to preferences for next boot\n", versionToLog);
        } else if (strcmp(status, "success") == 0) {
            this->preferences.begin("iotnet", false);
            this->preferences.putBool("pubFailed", false);  // Ensure failed flag is cleared on success
            // Note: justUpdated is handled separately when OTA completes
            this->preferences.end();
            Serial.printf("Saved success status for version %s\n", versionToLog);
        }
    } else {
        char versionInPayload[32]; 
        if (strcmp(status, "failed") == 0 || strcmp(status, "pending") == 0 || 
            strcmp(status, "active") == 0 || strcmp(status, "success") == 0) {
            this->preferences.begin("iotnet", false);
            String targetVersion = this->preferences.getString("targetVer", "");
            this->preferences.end();
            
            if (targetVersion.length() > 0) {
                strncpy(versionInPayload, targetVersion.c_str(), sizeof(versionInPayload) - 1);
                versionInPayload[sizeof(versionInPayload) - 1] = '\0';
            } else {
                strncpy(versionInPayload, currentFirmwareVersion, sizeof(versionInPayload) - 1);
                versionInPayload[sizeof(versionInPayload) - 1] = '\0';
            }
        } else {
            strncpy(versionInPayload, currentFirmwareVersion, sizeof(versionInPayload) - 1);
            versionInPayload[sizeof(versionInPayload) - 1] = '\0';
        }
        
        Serial.printf("Update Status: %s (Version: %s)\n", status, versionInPayload);
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

    this->preferences.begin("iotnet", false);
    String targetVersion = this->preferences.getString("targetVer", "");
    this->preferences.end();
    
    char payload[256];
    if (targetVersion.length() > 0) {
        snprintf(payload, sizeof(payload), "{\"version\":\"%s\"}", targetVersion.c_str());
    } 

    if (!mqttClient.connected()) {
        Serial.println("Cannot register board: MQTT not connected");
        
        if (reconnectMQTT()) {
            Serial.println("MQTT reconnected successfully in registerBoard");
        } else {
            Serial.println("Failed to reconnect MQTT in registerBoard");
            return;
        }
    }

    bool success = mqttClient.publish(topic, (const uint8_t*)payload, strlen(payload), false); 
    if (!success) {
        Serial.printf("Failed to publish board registration to topic: %s\n", topic);
        delay(100);
        success = mqttClient.publish(topic, (const uint8_t*)payload, strlen(payload), false);
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

void IotNetESP32::configureTime(const char* timezone, const char* ntpServer1, 
                               const char* ntpServer2, const char* ntpServer3) {
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

String IotNetESP32::getFormattedTime(const char* format) {
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
        return mqttClient.publish(pins[pinIndex].topic, (const uint8_t*)valueStr, strlen(valueStr), true);
    } else {
        return mqttClient.publish(pins[pinIndex].topic, (const uint8_t*)valueStr, strlen(valueStr), false);
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