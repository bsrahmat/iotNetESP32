#ifndef IOTNET_OTA_UPDATE_SERVICE_H
#define IOTNET_OTA_UPDATE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "ota/OtaSessionState.h"

namespace iotnetesp32::ota {

struct OtaTriggerData {
    char otaId[OtaSessionState::OTA_ID_SIZE];
    char version[OtaSessionState::VERSION_SIZE];
    long nonce;
};

enum class SessionResponseStatus {
    InvalidPayload,
    CidMismatch,
    Ready
};

class OtaUpdateService {
  public:
    static bool parseTriggerPayload(const char *payload, OtaTriggerData *outTrigger);

    static bool storePendingSession(
        OtaSessionState &session,
        const OtaTriggerData &trigger,
        unsigned long requestTimeMs
    );

    static bool buildSessionRequestPayload(
        OtaSessionState &session,
        uint32_t randomPart1,
        uint32_t randomPart2,
        char *outPayload,
        size_t outPayloadSize
    );

    static SessionResponseStatus consumeSessionResponse(
        OtaSessionState &session,
        const char *payload,
        int *outExpiresIn
    );

    static bool fetchOtaUrl(
        const char *backendBaseUrl,
        const char *sessionKey,
        const char *otaId,
        long nonce,
        const char *version,
        char *outOtaUrl,
        size_t outOtaUrlSize
    );
};

}

#endif
