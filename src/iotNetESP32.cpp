#include "iotNetESP32.h"

TaskHandle_t otaTaskHandle = NULL;

iotNetESP32::iotNetESP32()
    : mqttHandler(mqttClient), stateMux(portMUX_INITIALIZER_UNLOCKED),
      mqttUsername(nullptr), mqttPassword(nullptr), dashboardId(nullptr),
      signature(nullptr), nonce(nullptr) {setupSSL();}

iotNetESP32::DeviceState::DeviceState() : topicCount(0), updateRequired(false) {}

void iotNetESP32::setupWiFi(const char* ssid, const char* password) {
    Serial.print(F("Connecting to WiFi "));
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }

    Serial.println(F("\nConnected to WiFi"));
}

void iotNetESP32::setupSSL() {
    Serial.println(F("Setting up SSL..."));
    apiClient.setCACert(var_4);
    mqttClient.setCACert(var_6);
    mqttClient.setPrivateKey(var_8);
    mqttClient.setCertificate(var_7);
    storageClient.setCACert(var_5);
    Serial.println(F("SSL setup complete"));
    Serial.println(F("=========="));
}

void iotNetESP32::setupMQTT(const char* username, const char* password, const char* dashboard) {
    mqttUsername = username;
    mqttPassword = password; 
    dashboardId = dashboard;
}

bool iotNetESP32::addVirtualPin(const char* pin, const char* pinName) {
    if (state.topicCount >= MAX_VIRTUAL_PINS) {
        Serial.println(F("Error: Maximum number of topics reached"));
        return false;
    }

    Serial.print(F("Connecting to API for pin "));
    Serial.println(pin);

    String url = String(var_3) + "/dashboards/" + dashboardId + "/pins/" + pin + "?secret_key=" + mqttPassword;
    if (!apiHttps.begin(apiClient, url)) {
        Serial.println(F("Error: Unable to connect to API"));
        return false;
    }

    int httpCode = apiHttps.GET();
    if (httpCode <= 0) {
        apiHttps.end();
        return false;
    }

    String response = apiHttps.getString();
    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, response);
    
    if (error) {
        Serial.println(F("Error: JSON parsing failed"));
        apiHttps.end();
        return false;
    }

    if (httpCode == HTTP_CODE_OK) {
        const char* widgetId = jsonDoc["data"]["widget"]["id"];
        state.dynamicTopics[state.topicCount] = String("/device/") + String(widgetId);
        state.virtualPins[state.topicCount] = String(pin);
        state.topicCount++;

        Serial.print(F("Widget ID: "));
        Serial.print(widgetId);
        Serial.println(F("\n=========="));
    } else {
        Serial.println(F("Pin not found"));
        const char* errorMsg = jsonDoc["errors"];
        Serial.print(F("Error message: "));
        Serial.print(errorMsg);
        Serial.println(F("\n=========="));
    }

    apiHttps.end();
    return true;
}

void iotNetESP32::validateDevice() {
    Serial.println(F("Validating device..."));

    String url = String(var_3) + "/devices/validate";
    url += "?secret_key=" + String(mqttPassword);
    url += "&nonce=" + String(nonce); 
    url += "&blockchain_signature=" + String(signature);

    if (!apiHttps.begin(apiClient, url)) {
        Serial.println(F("Error: Unable to connect to validation endpoint"));
        return;
    }

    int httpCode = apiHttps.GET();
    if (httpCode <= 0) {
        Serial.println(F("Error: Failed to validate device"));
        apiHttps.end();
        return;
    }

    String response = apiHttps.getString();
    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, response);

    if (error) {
        Serial.println(F("Error: JSON parsing failed"));
        apiHttps.end();
        return;
    }

    if (httpCode == HTTP_CODE_OK) {
        Serial.println(F("Device validated successfully"));
        state.urlOta = jsonDoc["data"]["url"].as<String>();
        state.version = jsonDoc["data"]["version"].as<String>();
        state.updateRequired = true;
    } else {
        const char* errorMsg = jsonDoc["errors"];
        Serial.print(F("Validation failed. Error: "));
        Serial.println(errorMsg);
    }

    apiHttps.end();
}

void iotNetESP32::updateStatusVersions(const char* status) {
    Serial.println(F("Updating status versions..."));

    String url = String(var_3) + "/devices/versions/" + state.version;
    url += "?secret_key=" + String(mqttPassword);
    url += "&status=" + String(status);

    if (!apiHttps.begin(apiClient, url)) {
        Serial.println(F("Error: Unable to connect to versions endpoint"));
        return;
    }

    int httpCode = apiHttps.GET();
    Serial.print(F("status: "));
    Serial.println(status);
    if (httpCode <= 0) {
        Serial.println(F("Error: Failed to fetch versions"));
        apiHttps.end();
        return;
    }

    String response = apiHttps.getString();
    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, response);

    if (error) {
        Serial.println(F("Error: JSON parsing failed"));
        apiHttps.end();
        return;
    }

    if (httpCode != HTTP_CODE_OK) {
        const char* errorMsg = jsonDoc["errors"];
        Serial.print(F("Failed to fetch versions. Error: "));
        Serial.println(errorMsg);
    }

    apiHttps.end();
}

int iotNetESP32::virtualPinInteraction(const char* pin) {
    for (int i = 0; i < state.topicCount; i++) {
        if (String(pin) == state.virtualPins[i]) {
            return state.virtualPinValues[i];
        }
    }
    return -1;
}

int iotNetESP32::virtualPinVisualization(const char* pin, int value) {
    return updateVirtualPin(pin, String(value).c_str(), value);
}

int iotNetESP32::virtualPinVisualization(const char* pin, float value) {
    int intValue = (int)(value * 100);
    return updateVirtualPin(pin, String(value, 2).c_str(), intValue);
}

int iotNetESP32::virtualPinVisualization(const char* pin, const char* value) {
    return updateVirtualPin(pin, value, atoi(value));
}

int iotNetESP32::updateVirtualPin(const char* pin, const char* pubValue, int storeValue) {
    for (int i = 0; i < state.topicCount; i++) {
        if (String(pin) == state.virtualPins[i]) {
            state.virtualPinValues[i] = storeValue;
            mqttHandler.publish(state.dynamicTopics[i].c_str(), pubValue);
            return storeValue;
        }
    }
    return -1;
}

void iotNetESP32::performFirmwareUpdate() {
    Serial.print(F("Starting firmware update...\nURL: "));
    Serial.println(state.urlOta.c_str());
    updateStatusVersions("live");
    
    if (!storageHttps.begin(storageClient, state.urlOta)) {
        updateStatusVersions("failed");
        Serial.println(F("Error: Unable to connect to storage"));
        return;
    }

    int httpCode = storageHttps.GET();
    if (httpCode != HTTP_CODE_OK) {
        updateStatusVersions("failed");
        Serial.print(F("Error downloading firmware: "));
        Serial.println(httpCode);
        storageHttps.end();
        return;
    }

    int contentLength = storageHttps.getSize();
    Serial.print(F("Content Length: "));
    Serial.println(contentLength);

    if (contentLength <= 0) {
        updateStatusVersions("failed");
        Serial.println(F("Error: Invalid content length"));
        storageHttps.end();
        return;
    }

    if (!Update.begin(contentLength)) {
        Serial.println(F("Error: Not enough space for update"));
        updateStatusVersions("failed");
        storageHttps.end();
        return;
    }

    WiFiClient* stream = storageHttps.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    Serial.print(F("Written: "));
    Serial.print(written);
    Serial.print(F("/"));
    Serial.print(contentLength);
    Serial.println(F(" bytes"));

    if (Update.end()) {
        if (Update.isFinished()) {
            Serial.println(F("Update successful. Rebooting..."));
            updateStatusVersions("success");
            ESP.restart();
        } else {
            Serial.println(F("Error: Update not finished"));
            updateStatusVersions("failed");
        }
    } else {
        Serial.print(F("Error occurred. Error #: "));
        Serial.println(Update.getError());
        updateStatusVersions("failed");
    }

    storageHttps.end();
}

void iotNetESP32::handleMQTTCallback(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    String deviceTopic = String("/device/") + String(mqttUsername) + "/" + String(mqttPassword);
    
    if (String(topic) == deviceTopic) {
        JsonDocument jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, message);
        
        if (!error) {
            nonce = jsonDoc["nonce"] | "";
            signature = jsonDoc["signature"] | "";
            
            if (strlen(nonce) > 0 && strlen(signature) > 0) {
                validateDevice();
                return; 
            }
        }
    }

    for (int i = 0; i < state.topicCount; i++) {
        if (String(topic) == state.dynamicTopics[i]) {
            state.virtualPinValues[i] = atoi(message);
            return;
        }
    }

    Serial.print(F("Warning: Unknown topic: "));
    Serial.println(topic);
}

void otaTask(void *pvParameters) {
    iotNetESP32* tunnel = (iotNetESP32*)pvParameters;
    
    for(;;) {
        portENTER_CRITICAL(&tunnel->stateMux);
        bool updateReq = tunnel->state.updateRequired;
        portEXIT_CRITICAL(&tunnel->stateMux);

        if (updateReq) {
            tunnel->performFirmwareUpdate();
            portENTER_CRITICAL(&tunnel->stateMux);
            tunnel->state.updateRequired = false;
            portEXIT_CRITICAL(&tunnel->stateMux);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void iotNetESP32::setup() {
    Serial.println(F("Setting up MQTT connection..."));

    mqttHandler.setServer(var_2, var_1);
    mqttHandler.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->handleMQTTCallback(topic, payload, length);
    });

    char clientId[32];
    snprintf(clientId, sizeof(clientId), "ESP32Client-%llX", ESP.getEfuseMac());
    
    if (!mqttHandler.connect(clientId, mqttUsername, mqttPassword)) {
        Serial.print(F("MQTT connection failed, rc="));
        Serial.println(mqttHandler.state());
        return;
    }

    Serial.println(F("Connected to MQTT broker"));

    String deviceTopic = String("/device/") + String(mqttUsername) + "/" + String(mqttPassword);
    mqttHandler.subscribe(deviceTopic.c_str());

    for (int i = 0; i < state.topicCount; i++) {
        if (!state.dynamicTopics[i].isEmpty()) {
            if (mqttHandler.subscribe(state.dynamicTopics[i].c_str())) {
                Serial.print(F("Subscribed to topic: "));
                Serial.println(state.dynamicTopics[i].c_str());
            } else {
                Serial.print(F("Failed to subscribe to topic: "));
                Serial.print(state.dynamicTopics[i].c_str());
                Serial.print(F(" (Error: "));
                Serial.print(mqttHandler.state());
                Serial.println(F(")"));
            }
        }
    }

    xTaskCreate(
        otaTask,  
        "OTA Task",  
        configMINIMAL_STACK_SIZE * 6, 
        this,       
        0,        
        &otaTaskHandle 
    );

    Serial.println(F("=========="));
}

void iotNetESP32::loop() {
    if (!mqttHandler.connected()) {
        setupMQTT(mqttUsername, mqttPassword, dashboardId);
    }
    mqttHandler.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
