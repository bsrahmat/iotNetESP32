#ifndef IOTNET_CORE_CLIENT_CONFIG_H
#define IOTNET_CORE_CLIENT_CONFIG_H

// Core shared client configuration contract (feature boundary)
// This header replaces the legacy inline ClientConfig struct in IotNetESP32
// and serves as the public surface for runtime consumer config.
struct ClientConfig {
  const char *mqttUsername;
  const char *mqttPassword;
  const char *boardName;
  const char *firmwareVersion;
  bool enableOta;
};

#endif
