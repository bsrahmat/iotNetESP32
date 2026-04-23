#include "IotNetESP32.h"

#include "core/TopicBuilder.h"
#include "i-ot.net.h"
#include "ota/FirmwareFlasher.h"
#include "ota/OtaUpdateService.h"
#include <esp_system.h>

void IotNetESP32::enableOtaUpdates() {
    otaUpdatesEnabled = true;
    if (mqttClient.connected()) {
        subscribeToOtaUpdates();
    }
    Serial.println("[OTA] OTA updates enabled");
}

bool IotNetESP32::isOtaInProgress() const {
    return otaInProgress;
}

void IotNetESP32::subscribeToOtaUpdates() {
    if (!credentials.mqttUsername || !credentials.boardIdentifier) {
        Serial.println("[OTA] FAIL: Cannot subscribe - credentials not set");
        return;
    }

    if (!iotnet::core::buildDeviceTopic(
            otaTopic,
            sizeof(otaTopic),
            credentials.mqttUsername,
            credentials.boardIdentifier,
            "ota/update"
        ) ||
        !iotnet::core::buildDeviceTopic(
            otaSessionRequestTopic,
            sizeof(otaSessionRequestTopic),
            credentials.mqttUsername,
            credentials.boardIdentifier,
            "ota/session/request"
        ) ||
        !iotnet::core::buildDeviceTopic(
            otaSessionResponseTopic,
            sizeof(otaSessionResponseTopic),
            credentials.mqttUsername,
            credentials.boardIdentifier,
            "ota/session/response"
        )) {
        Serial.println("[OTA] FAIL: Unable to build OTA topics");
        return;
    }

    if (mqttClient.subscribe(otaTopic)) {
        Serial.printf("[OTA] Subscribed to trigger: %s\n", otaTopic);
    } else {
        Serial.printf("[OTA] FAIL: Subscribe to trigger topic failed: %s\n", otaTopic);
    }

    if (mqttClient.subscribe(otaSessionResponseTopic)) {
        Serial.printf("[OTA] Subscribed to session response: %s\n", otaSessionResponseTopic);
    } else {
        Serial.printf("[OTA] FAIL: Subscribe to response topic failed: %s\n", otaSessionResponseTopic);
    }
}

void IotNetESP32::handleOtaMessage(const char *payload) {
    if (!payload || strlen(payload) == 0) {
        Serial.println("[OTA-TRIGGER] FAIL: Empty payload");
        return;
    }

    Serial.printf("[OTA-TRIGGER] Received: %s\n", payload);

    iotnetesp32::ota::OtaTriggerData trigger{};
    if (!iotnetesp32::ota::OtaUpdateService::parseTriggerPayload(payload, &trigger)) {
        Serial.println("[OTA-TRIGGER] FAIL: Invalid trigger payload");
        return;
    }

    Serial.printf(
        "[OTA-TRIGGER] OK: version=%s ota_id=%s nonce=%ld\n",
        trigger.version,
        trigger.otaId,
        trigger.nonce
    );

    if (!iotnetesp32::ota::OtaUpdateService::storePendingSession(otaSession, trigger, millis())) {
        Serial.println("[OTA-TRIGGER] FAIL: Cannot store pending OTA state");
        return;
    }

    requestOtaSessionKey();
}

void IotNetESP32::requestOtaSessionKey() {
    if (!mqttClient.connected()) {
        Serial.println("[OTA-SESSION] FAIL: MQTT not connected, cannot publish request");
        otaSession.setWaiting(false);
        updateBoardStatusInternal("failed");
        return;
    }

    char requestPayload[256];
    if (!iotnetesp32::ota::OtaUpdateService::buildSessionRequestPayload(
            otaSession,
            esp_random(),
            esp_random(),
            requestPayload,
            sizeof(requestPayload)
        )) {
        Serial.println("[OTA-SESSION] FAIL: Cannot build request payload");
        otaSession.setWaiting(false);
        updateBoardStatusInternal("failed");
        return;
    }

    Serial.printf("[OTA-SESSION] Publishing request on: %s\n", otaSessionRequestTopic);

    bool success = mqttClient.publish(
        otaSessionRequestTopic,
        (const uint8_t *)requestPayload,
        strlen(requestPayload),
        false
    );
    if (!success) {
        Serial.println("[OTA-SESSION] FAIL: Publish failed");
        otaSession.setWaiting(false);
        updateBoardStatusInternal("failed");
    } else {
        Serial.printf("[OTA-SESSION] OK: Request published, cid=%s\n", otaSession.correlationId());
    }
}

void IotNetESP32::handleOtaSessionResponse(const char *payload) {
    if (!payload || strlen(payload) == 0) {
        Serial.println("[OTA-SESSION] FAIL: Empty response payload");
        return;
    }

    if (!otaSession.isWaiting()) {
        Serial.println("[OTA-SESSION] FAIL: Received response but not waiting for one");
        return;
    }

    Serial.println("[OTA-SESSION] Response received");

    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus responseStatus =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(otaSession, payload, &expiresIn);
    if (responseStatus == iotnetesp32::ota::SessionResponseStatus::InvalidPayload) {
        Serial.println("[OTA-SESSION] FAIL: Invalid response payload");
        otaSession.setWaiting(false);
        updateBoardStatusInternal("failed");
        return;
    }

    if (responseStatus == iotnetesp32::ota::SessionResponseStatus::CidMismatch) {
        Serial.printf(
            "[OTA-SESSION] FAIL: CID mismatch (expected=%s got=%s)\n",
            otaSession.correlationId(),
            "mismatch"
        );
        return;
    }

    Serial.printf(
        "[OTA-SESSION] OK: Key received (cid=%s, expires=%ds)\n",
        otaSession.correlationId(),
        expiresIn
    );
    otaSession.setWaiting(false);

    bool success = fetchOtaLinkWithSessionKey(
        otaSession.currentSessionKey(),
        otaSession.otaId(),
        otaSession.nonce(),
        otaSession.version()
    );
    otaSession.clearSessionKey();
    if (!success) {
        Serial.println("[OTA-SESSION] FAIL: Could not fetch OTA link");
        updateBoardStatusInternal("failed");
    }
}

bool IotNetESP32::fetchOtaLinkWithSessionKey(
    const char *sessionKey,
    const char *otaId,
    long nonce,
    const char *version
) {
    Serial.println("[OTA-LINK] Fetching OTA link with session key...");

    char otaUrl[256];
    if (!iotnetesp32::ota::OtaUpdateService::fetchOtaUrl(
            var_3,
            sessionKey,
            otaId,
            nonce,
            version,
            otaUrl,
            sizeof(otaUrl)
        )) {
        Serial.println("[OTA-LINK] FAIL: Invalid response payload");
        return false;
    }

    Serial.printf("[OTA-LINK] OK: URL obtained (%zu bytes)\n", strlen(otaUrl));

    bool success = downloadAndFlashFirmware(otaUrl);
    if (success) {
        Serial.println("[OTA-LINK] Update successful! Rebooting...");
        updateBoardStatusInternal("success");
        delay(1000);
        ESP.restart();
    } else {
        Serial.println("[OTA-LINK] FAIL: Firmware flash failed");
        updateBoardStatusInternal("failed");
    }
    return success;
}

bool IotNetESP32::downloadAndFlashFirmware(const char *url) {
    otaInProgress = true;
    bool success = iotnetesp32::ota::FirmwareFlasher::downloadAndFlash(url);
    otaInProgress = false;
    return success;
}
