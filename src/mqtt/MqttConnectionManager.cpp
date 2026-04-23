#include "mqtt/MqttConnectionManager.h"

namespace iotnetesp32::mqtt {

bool MqttConnectionManager::isServerConfigValid(const char *server, int port) {
    return server && port > 0;
}

void MqttConnectionManager::configureTransport(
    PubSubClient &client,
    const char *server,
    int port,
    uint16_t keepAliveSeconds,
    uint16_t socketTimeoutSeconds,
    uint16_t bufferSizeBytes
) {
    client.setServer(server, port);
    client.setKeepAlive(keepAliveSeconds);
    client.setSocketTimeout(socketTimeoutSeconds);
    client.setBufferSize(bufferSizeBytes);
}

bool MqttConnectionManager::connectWithLwt(
    PubSubClient &client,
    const char *clientId,
    const char *username,
    const char *password,
    const char *lwtTopic,
    const char *lwtPayload,
    uint8_t lwtQos,
    bool lwtRetained
) {
    if (!clientId || !username || !password || !lwtTopic || !lwtPayload) {
        return false;
    }

    return client.connect(clientId, username, password, lwtTopic, lwtQos, lwtRetained, lwtPayload);
}

}
