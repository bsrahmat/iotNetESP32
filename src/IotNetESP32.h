#ifndef IOTNET_ESP32_H
#define IOTNET_ESP32_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class IotNetESP32 {
  public:
    static constexpr int MAX_PINS = 50;
    static constexpr unsigned long RECONNECT_DELAY_MS = 5000;
    static constexpr unsigned long WIFI_TIMEOUT_MS = 30000;
    static constexpr unsigned long MQTT_TIMEOUT_MS = 60000;

    struct PinState {
        char topic[120];
        char value[32];
        bool updated;
        bool initialized;
    };

    struct PinCallback {
        int pinIndex;
        void (*callback)(String);
    };

    struct OtaUpdateInfo {
        char signature[136];
        char version[16];
        uint32_t nonce;
    };

    IotNetESP32();

    void run();
    void begin();
    void connect();
    void version(const char *version);
    const char *version() const;
    void setStatusPin(int pin);
    void setMQTTServer();

    bool shouldUpdate(unsigned long &lastUpdate, unsigned long interval);
    bool hasNewValue(const char *pin);
    void registerCallback(const char *pin, void (*callback)(String));

    template <typename T> bool virtualWrite(const char *pin, T value);
    template <typename T> T virtualRead(const char *pin);

  private:
    struct NetworkCredentials {
        const char *mqttUsername;
        const char *mqttPassword;
        const char *boardName;
    };

    struct MqttConfig {
        const char *server;
        int port;
        int statusPin;
    };

    struct CertificateInfo {
        const char *caCert;
        const char *clientCert;
        const char *clientKey;
    };

    WiFiClientSecure espClient;
    PubSubClient mqttClient;

    NetworkCredentials credentials;
    MqttConfig mqttConfig;
    CertificateInfo certificates;
    char currentFirmwareVersion[16];

    PinState pins[MAX_PINS];
    PinCallback callbacks[MAX_PINS];
    int numCallbacks;

    void setMQTTCredentials(const char *username, const char *password);
    void setCertificates();
    void setupCertificates();

    void checkConnections();
    bool reconnectMQTT();
    void printLogo();

    int convertPinToIndex(const char *pin);
    void initPinTopic(int pin);

    static void staticMqttCallback(char *topic, byte *payload, unsigned int length);
    void mqttCallback(char *topic, byte *payload, unsigned int length);

    void processOtaUpdate(const char *otaMessage);
    static void otaUpdateTask(void *parameters);
    void subscribeToOtaUpdates();
    void updateBoardStatus(const char *status);
    void registerBoard();

    size_t getFreeHeap();

    template <typename T> bool publishToPin(const char *pin, T value);
    template <typename T> const char *toString(T value, char *buffer, size_t bufferSize);
    template <typename T> T fromString(const String &str);
};

// toString specializations
template <> const char *IotNetESP32::toString<float>(float value, char *buffer, size_t bufferSize);
template <>
const char *IotNetESP32::toString<double>(double value, char *buffer, size_t bufferSize);
template <> const char *IotNetESP32::toString<int>(int value, char *buffer, size_t bufferSize);
template <>
const char *IotNetESP32::toString<unsigned int>(unsigned int value, char *buffer,
                                                size_t bufferSize);
template <> const char *IotNetESP32::toString<long>(long value, char *buffer, size_t bufferSize);
template <>
const char *IotNetESP32::toString<unsigned long>(unsigned long value, char *buffer,
                                                 size_t bufferSize);
template <>
const char *IotNetESP32::toString<const char *>(const char *value, char *buffer, size_t bufferSize);
template <> const char *IotNetESP32::toString<bool>(bool value, char *buffer, size_t bufferSize);
template <>
const char *IotNetESP32::toString<String>(String value, char *buffer, size_t bufferSize);

// fromString specializations
template <> int IotNetESP32::fromString<int>(const String &str);
template <> unsigned int IotNetESP32::fromString<unsigned int>(const String &str);
template <> long IotNetESP32::fromString<long>(const String &str);
template <> unsigned long IotNetESP32::fromString<unsigned long>(const String &str);
template <> float IotNetESP32::fromString<float>(const String &str);
template <> double IotNetESP32::fromString<double>(const String &str);
template <> bool IotNetESP32::fromString<bool>(const String &str);
template <> String IotNetESP32::fromString<String>(const String &str);
template <> char *IotNetESP32::fromString<char *>(const String &str);

// External template declarations for publishToPin
extern template bool IotNetESP32::publishToPin<int>(const char *pin, int value);
extern template bool IotNetESP32::publishToPin<float>(const char *pin, float value);
extern template bool IotNetESP32::publishToPin<double>(const char *pin, double value);
extern template bool IotNetESP32::publishToPin<bool>(const char *pin, bool value);
extern template bool IotNetESP32::publishToPin<unsigned int>(const char *pin, unsigned int value);
extern template bool IotNetESP32::publishToPin<long>(const char *pin, long value);
extern template bool IotNetESP32::publishToPin<unsigned long>(const char *pin, unsigned long value);
extern template bool IotNetESP32::publishToPin<const char *>(const char *pin, const char *value);
extern template bool IotNetESP32::publishToPin<String>(const char *pin, String value);

// External template declarations for virtualWrite
extern template bool IotNetESP32::virtualWrite<int>(const char *pin, int value);
extern template bool IotNetESP32::virtualWrite<float>(const char *pin, float value);
extern template bool IotNetESP32::virtualWrite<double>(const char *pin, double value);
extern template bool IotNetESP32::virtualWrite<bool>(const char *pin, bool value);
extern template bool IotNetESP32::virtualWrite<unsigned int>(const char *pin, unsigned int value);
extern template bool IotNetESP32::virtualWrite<long>(const char *pin, long value);
extern template bool IotNetESP32::virtualWrite<unsigned long>(const char *pin, unsigned long value);
extern template bool IotNetESP32::virtualWrite<const char *>(const char *pin, const char *value);
extern template bool IotNetESP32::virtualWrite<String>(const char *pin, String value);

// External template declarations for virtualRead
extern template int IotNetESP32::virtualRead<int>(const char *pin);
extern template float IotNetESP32::virtualRead<float>(const char *pin);
extern template double IotNetESP32::virtualRead<double>(const char *pin);
extern template bool IotNetESP32::virtualRead<bool>(const char *pin);
extern template unsigned int IotNetESP32::virtualRead<unsigned int>(const char *pin);
extern template long IotNetESP32::virtualRead<long>(const char *pin);
extern template unsigned long IotNetESP32::virtualRead<unsigned long>(const char *pin);
extern template String IotNetESP32::virtualRead<String>(const char *pin);

#endif
