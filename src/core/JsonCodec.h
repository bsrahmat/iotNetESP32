#ifndef IOTNET_JSON_CODEC_H
#define IOTNET_JSON_CODEC_H

#include <stddef.h>

namespace iotnet::core {

bool parseOtaTriggerPayload(
    const char *payload,
    char *outOtaId,
    size_t outOtaIdSize,
    char *outVersion,
    size_t outVersionSize,
    long *outNonce
);

bool buildOtaSessionRequestPayload(
    char *outPayload,
    size_t outPayloadSize,
    const char *otaId,
    long nonce,
    const char *correlationId
);

bool parseOtaSessionResponsePayload(
    const char *payload,
    char *outCorrelationId,
    size_t outCorrelationIdSize,
    char *outSessionKey,
    size_t outSessionKeySize,
    int *outExpiresIn
);

bool buildOtaLinkRequestPayload(
    char *outPayload,
    size_t outPayloadSize,
    const char *otaId,
    long nonce,
    const char *version
);

bool parseOtaLinkResponsePayload(
    const char *payload,
    char *outOtaUrl,
    size_t outOtaUrlSize
);

}

#endif
