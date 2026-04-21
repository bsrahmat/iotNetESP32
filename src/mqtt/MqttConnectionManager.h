#ifndef IOTNET_MQTT_CONNECTION_MANAGER_H
#define IOTNET_MQTT_CONNECTION_MANAGER_H

#include <PubSubClient.h>
#include <stdint.h>

namespace iotnetesp32::mqtt {

class MqttConnectionManager {
  public:
    static bool isServerConfigValid(const char *server, int port);

    static void configureTransport(
        PubSubClient &client,
        const char *server,
        int port,
        uint16_t keepAliveSeconds,
        uint16_t socketTimeoutSeconds,
        uint16_t bufferSizeBytes
    );

    static bool connectWithLwt(
        PubSubClient &client,
        const char *clientId,
        const char *username,
        const char *password,
        const char *lwtTopic,
        const char *lwtPayload,
        uint8_t lwtQos,
        bool lwtRetained
    );
};

}

#endif
