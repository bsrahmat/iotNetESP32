#ifndef IOTNET_TOPIC_BUILDER_H
#define IOTNET_TOPIC_BUILDER_H

#include <stddef.h>

namespace iotnet::core {

bool buildDeviceTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardIdentifier,
    const char *channel
);

bool buildPinTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardIdentifier,
    int pin
);

}

#endif
