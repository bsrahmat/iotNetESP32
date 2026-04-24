#include "core/TopicBuilder.h"

#include <stdio.h>

namespace iotnet::core {

bool buildDeviceTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardIdentifier,
    const char *channel
) {
    if (!outTopic || outSize == 0 || !deviceId || !boardIdentifier || !channel) {
        return false;
    }

    int written = snprintf(outTopic, outSize, "devices/%s/%s/%s", deviceId, boardIdentifier, channel);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

bool buildPinTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardIdentifier,
    int pin
) {
    if (!outTopic || outSize == 0 || !deviceId || !boardIdentifier) {
        return false;
    }

    int written = snprintf(outTopic, outSize, "devices/%s/%s/V%d", deviceId, boardIdentifier, pin);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

}
