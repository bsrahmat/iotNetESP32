#include "ota/OtaUpdateService.h"

#include <stdio.h>
#include <string.h>

#include "core/JsonCodec.h"
#ifdef ARDUINO
#include <HTTPClient.h>
#endif

namespace iotnetesp32::ota {

bool OtaUpdateService::parseTriggerPayload(const char *payload, OtaTriggerData *outTrigger) {
    if (!outTrigger) {
        return false;
    }

    outTrigger->nonce = 0;
    outTrigger->otaId[0] = '\0';
    outTrigger->version[0] = '\0';

    return iotnet::core::parseOtaTriggerPayload(
        payload,
        outTrigger->otaId,
        sizeof(outTrigger->otaId),
        outTrigger->version,
        sizeof(outTrigger->version),
        &outTrigger->nonce
    );
}

bool OtaUpdateService::storePendingSession(
    OtaSessionState &session,
    const OtaTriggerData &trigger,
    unsigned long requestTimeMs
) {
    return session.setPending(trigger.otaId, trigger.version, trigger.nonce, requestTimeMs);
}

bool OtaUpdateService::buildSessionRequestPayload(
    OtaSessionState &session,
    uint32_t randomPart1,
    uint32_t randomPart2,
    char *outPayload,
    size_t outPayloadSize
) {
    if (!outPayload || outPayloadSize == 0) {
        return false;
    }

    char correlationId[OtaSessionState::CORRELATION_ID_SIZE];
    int writtenCid = snprintf(
        correlationId,
        sizeof(correlationId),
        "%08lx%08lx",
        static_cast<unsigned long>(randomPart1),
        static_cast<unsigned long>(randomPart2)
    );
    if (writtenCid <= 0 || static_cast<size_t>(writtenCid) >= sizeof(correlationId)) {
        return false;
    }

    if (!session.setCorrelationId(correlationId)) {
        return false;
    }

    return iotnet::core::buildOtaSessionRequestPayload(
        outPayload,
        outPayloadSize,
        session.otaId(),
        session.nonce(),
        session.correlationId()
    );
}

SessionResponseStatus OtaUpdateService::consumeSessionResponse(
    OtaSessionState &session,
    const char *payload,
    int *outExpiresIn
) {
    if (!outExpiresIn) {
        return SessionResponseStatus::InvalidPayload;
    }
    *outExpiresIn = 0;

    char responseCid[OtaSessionState::CORRELATION_ID_SIZE];
    char responseSessionKey[OtaSessionState::SESSION_KEY_SIZE];
    if (!iotnet::core::parseOtaSessionResponsePayload(
            payload,
            responseCid,
            sizeof(responseCid),
            responseSessionKey,
            sizeof(responseSessionKey),
            outExpiresIn
        )) {
        return SessionResponseStatus::InvalidPayload;
    }

    if (!session.matchesCorrelationId(responseCid)) {
        return SessionResponseStatus::CidMismatch;
    }

    if (!session.setSessionKey(responseSessionKey)) {
        return SessionResponseStatus::InvalidPayload;
    }

    return SessionResponseStatus::Ready;
}

bool OtaUpdateService::fetchOtaUrl(
    const char *backendBaseUrl,
    const char *sessionKey,
    const char *otaId,
    long nonce,
    const char *version,
    char *outOtaUrl,
    size_t outOtaUrlSize
) {
    if (!backendBaseUrl || !sessionKey || !otaId || !version || !outOtaUrl || outOtaUrlSize == 0) {
        return false;
    }

#ifndef ARDUINO
    (void)backendBaseUrl;
    (void)sessionKey;
    (void)otaId;
    (void)nonce;
    (void)version;
    (void)outOtaUrl;
    (void)outOtaUrlSize;
    return false;
#else
    char requestBody[512];
    if (!iotnet::core::buildOtaLinkRequestPayload(
            requestBody,
            sizeof(requestBody),
            otaId,
            nonce,
            version
        )) {
        return false;
    }

    HTTPClient http;
    char url[256];
    int written = snprintf(url, sizeof(url), "%s/v1/ota/versions/link", backendBaseUrl);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(url)) {
        return false;
    }

    if (!http.begin(url)) {
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-session-key", sessionKey);

    int httpCode = http.POST(requestBody);
    if (httpCode != 200) {
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    return iotnet::core::parseOtaLinkResponsePayload(response.c_str(), outOtaUrl, outOtaUrlSize);
#endif
}

}
