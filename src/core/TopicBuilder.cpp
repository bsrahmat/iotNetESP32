#include "core/TopicBuilder.h"

#include <stdio.h>

namespace iotnet::core {

bool buildDeviceTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardName,
    const char *channel
) {
    if (!outTopic || outSize == 0 || !deviceId || !boardName || !channel) {
        return false;
    }

    int written = snprintf(outTopic, outSize, "devices/%s/%s/%s", deviceId, boardName, channel);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

bool buildPinTopic(
    char *outTopic,
    size_t outSize,
    const char *deviceId,
    const char *boardName,
    int pin
) {
    if (!outTopic || outSize == 0 || !deviceId || !boardName) {
        return false;
    }

    int written = snprintf(outTopic, outSize, "devices/%s/%s/V%d", deviceId, boardName, pin);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

}
