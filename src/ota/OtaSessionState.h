#ifndef IOTNET_OTA_SESSION_STATE_H
#define IOTNET_OTA_SESSION_STATE_H

#include <stddef.h>
#include <string.h>

namespace iotnetesp32::ota {

class OtaSessionState {
  public:
    static constexpr size_t OTA_ID_SIZE = 48;
    static constexpr size_t VERSION_SIZE = 16;
    static constexpr size_t CORRELATION_ID_SIZE = 48;
    static constexpr size_t SESSION_KEY_SIZE = 128;

    OtaSessionState()
        : pendingNonce(0), waitingForSessionKey(false), sessionKeyRequestTimeMs(0) {
        reset();
    }

    void reset() {
        pendingOtaId[0] = '\0';
        pendingVersion[0] = '\0';
        pendingCorrelationId[0] = '\0';
        sessionKey[0] = '\0';
        pendingNonce = 0;
        waitingForSessionKey = false;
        sessionKeyRequestTimeMs = 0;
    }

    bool setPending(const char *otaId, const char *version, long nonce, unsigned long requestTimeMs) {
        if (!otaId || !version) {
            return false;
        }
        size_t otaIdLength = strlen(otaId);
        size_t versionLength = strlen(version);
        if (otaIdLength == 0 || otaIdLength >= sizeof(pendingOtaId) || versionLength == 0 ||
            versionLength >= sizeof(pendingVersion)) {
            return false;
        }

        memcpy(pendingOtaId, otaId, otaIdLength + 1);
        memcpy(pendingVersion, version, versionLength + 1);
        pendingNonce = nonce;
        waitingForSessionKey = true;
        sessionKeyRequestTimeMs = requestTimeMs;
        return true;
    }

    bool setCorrelationId(const char *cid) {
        if (!cid) {
            return false;
        }
        size_t cidLength = strlen(cid);
        if (cidLength == 0 || cidLength >= sizeof(pendingCorrelationId)) {
            return false;
        }
        memcpy(pendingCorrelationId, cid, cidLength + 1);
        return true;
    }

    bool matchesCorrelationId(const char *cid) const {
        return cid && strcmp(pendingCorrelationId, cid) == 0;
    }

    bool setSessionKey(const char *value) {
        if (!value) {
            return false;
        }
        size_t keyLength = strlen(value);
        if (keyLength == 0 || keyLength >= sizeof(sessionKey)) {
            return false;
        }
        memcpy(sessionKey, value, keyLength + 1);
        return true;
    }

    void clearSessionKey() { memset(sessionKey, 0, sizeof(sessionKey)); }

    bool isWaiting() const { return waitingForSessionKey; }
    void setWaiting(bool waiting) { waitingForSessionKey = waiting; }

    bool isTimedOut(unsigned long nowMs, unsigned long timeoutMs) const {
        return waitingForSessionKey && (nowMs - sessionKeyRequestTimeMs > timeoutMs);
    }

    const char *otaId() const { return pendingOtaId; }
    const char *version() const { return pendingVersion; }
    long nonce() const { return pendingNonce; }
    const char *correlationId() const { return pendingCorrelationId; }
    const char *currentSessionKey() const { return sessionKey; }

  private:
    char pendingOtaId[OTA_ID_SIZE];
    char pendingVersion[VERSION_SIZE];
    long pendingNonce;
    char pendingCorrelationId[CORRELATION_ID_SIZE];
    char sessionKey[SESSION_KEY_SIZE];
    bool waitingForSessionKey;
    unsigned long sessionKeyRequestTimeMs;
};

}

#endif
