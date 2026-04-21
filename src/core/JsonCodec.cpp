#include "core/JsonCodec.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace iotnet::core {

bool parseOtaTriggerPayload(
    const char *payload,
    char *outOtaId,
    size_t outOtaIdSize,
    char *outVersion,
    size_t outVersionSize,
    long *outNonce
) {
    if (!payload || !outOtaId || outOtaIdSize == 0 || !outVersion || outVersionSize == 0 ||
        !outNonce) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        return false;
    }

    if (!doc["ota_id"].is<const char *>() || !doc["version"].is<const char *>() ||
        !doc["nonce"].is<long>()) {
        return false;
    }

    const char *otaId = doc["ota_id"].as<const char *>();
    const char *version = doc["version"].as<const char *>();
    if (!otaId || !version) {
        return false;
    }

    size_t otaIdLength = strlen(otaId);
    size_t versionLength = strlen(version);
    if (otaIdLength >= outOtaIdSize || versionLength >= outVersionSize) {
        return false;
    }

    memcpy(outOtaId, otaId, otaIdLength + 1);
    memcpy(outVersion, version, versionLength + 1);
    *outNonce = doc["nonce"].as<long>();
    return true;
}

bool buildOtaSessionRequestPayload(
    char *outPayload,
    size_t outPayloadSize,
    const char *otaId,
    long nonce,
    const char *correlationId
) {
    if (!outPayload || outPayloadSize == 0 || !otaId || !correlationId) {
        return false;
    }

    int written = snprintf(
        outPayload,
        outPayloadSize,
        "{\"ota_id\":\"%s\",\"nonce\":%ld,\"cid\":\"%s\"}",
        otaId,
        nonce,
        correlationId
    );

    return written > 0 && static_cast<size_t>(written) < outPayloadSize;
}

bool parseOtaSessionResponsePayload(
    const char *payload,
    char *outCorrelationId,
    size_t outCorrelationIdSize,
    char *outSessionKey,
    size_t outSessionKeySize,
    int *outExpiresIn
) {
    if (!payload || !outCorrelationId || outCorrelationIdSize == 0 || !outSessionKey ||
        outSessionKeySize == 0 || !outExpiresIn) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        return false;
    }

    if (!doc["cid"].is<const char *>() || !doc["session_key"].is<const char *>()) {
        return false;
    }

    const char *correlationId = doc["cid"].as<const char *>();
    const char *sessionKey = doc["session_key"].as<const char *>();
    if (!correlationId || !sessionKey) {
        return false;
    }

    size_t cidLength = strlen(correlationId);
    size_t keyLength = strlen(sessionKey);
    if (cidLength == 0 || cidLength >= outCorrelationIdSize || keyLength == 0 ||
        keyLength >= outSessionKeySize) {
        return false;
    }

    memcpy(outCorrelationId, correlationId, cidLength + 1);
    memcpy(outSessionKey, sessionKey, keyLength + 1);
    *outExpiresIn = doc["expires_in"] | 0;
    return true;
}

bool buildOtaLinkRequestPayload(
    char *outPayload,
    size_t outPayloadSize,
    const char *otaId,
    long nonce,
    const char *version
) {
    if (!outPayload || outPayloadSize == 0 || !otaId || !version) {
        return false;
    }

    int written = snprintf(
        outPayload,
        outPayloadSize,
        "{\"ota_id\":\"%s\",\"nonce\":%ld,\"version\":\"%s\"}",
        otaId,
        nonce,
        version
    );

    return written > 0 && static_cast<size_t>(written) < outPayloadSize;
}

bool parseOtaLinkResponsePayload(const char *payload, char *outOtaUrl, size_t outOtaUrlSize) {
    if (!payload || !outOtaUrl || outOtaUrlSize == 0) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        return false;
    }

    const char *otaUrl = doc["data"]["ota_url"].as<const char *>();
    if (!otaUrl) {
        return false;
    }

    size_t urlLength = strlen(otaUrl);
    if (urlLength == 0 || urlLength >= outOtaUrlSize) {
        return false;
    }

    memcpy(outOtaUrl, otaUrl, urlLength + 1);
    return true;
}

}
