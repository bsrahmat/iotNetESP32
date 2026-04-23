#include <unity.h>
#include <stdio.h>
#include <string.h>

#include "core/JsonCodec.h"
#include "core/ClientConfig.h"
#include "ota/OtaSessionState.h"
#include "ota/OtaUpdateService.h"

void test_client_config_struct_initialization() {
    ClientConfig config = {
        .mqttUsername = "test_user",
        .mqttPassword = "test_pass",
        .boardIdentifier = "test_board",
        .firmwareVersion = "1.0.0",
        .enableOta = true
    };

    TEST_ASSERT_NOT_NULL(config.mqttUsername);
    TEST_ASSERT_NOT_NULL(config.mqttPassword);
    TEST_ASSERT_NOT_NULL(config.boardIdentifier);
    TEST_ASSERT_EQUAL_STRING("test_user", config.mqttUsername);
    TEST_ASSERT_EQUAL_STRING("test_pass", config.mqttPassword);
    TEST_ASSERT_EQUAL_STRING("test_board", config.boardIdentifier);
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.firmwareVersion);
    TEST_ASSERT_TRUE(config.enableOta);
}

void test_client_config_null_credentials() {
    ClientConfig config = {
        .mqttUsername = nullptr,
        .mqttPassword = nullptr,
        .boardIdentifier = nullptr,
        .firmwareVersion = nullptr,
        .enableOta = false
    };

    TEST_ASSERT_NULL(config.mqttUsername);
    TEST_ASSERT_NULL(config.mqttPassword);
    TEST_ASSERT_NULL(config.boardIdentifier);
    TEST_ASSERT_NULL(config.firmwareVersion);
    TEST_ASSERT_FALSE(config.enableOta);
}

void test_ota_session_state_lifecycle() {
    iotnetesp32::ota::OtaSessionState state;

    TEST_ASSERT_TRUE(state.setPending("ota-123", "1.2.3", 42, 1000));
    TEST_ASSERT_TRUE(state.isWaiting());
    TEST_ASSERT_EQUAL_STRING("ota-123", state.otaId());
    TEST_ASSERT_EQUAL_STRING("1.2.3", state.version());
    TEST_ASSERT_EQUAL_INT(42, state.nonce());

    TEST_ASSERT_TRUE(state.setCorrelationId("abcd1234"));
    TEST_ASSERT_TRUE(state.matchesCorrelationId("abcd1234"));
    TEST_ASSERT_FALSE(state.matchesCorrelationId("wrong"));

    TEST_ASSERT_TRUE(state.setSessionKey("session-key-xyz"));
    TEST_ASSERT_EQUAL_STRING("session-key-xyz", state.currentSessionKey());

    state.clearSessionKey();
    TEST_ASSERT_EQUAL_STRING("", state.currentSessionKey());

    TEST_ASSERT_TRUE(state.isTimedOut(5000, 3000));
}

void test_json_codec_trigger_payload_parse() {
    const char *payload = "{\"ota_id\":\"ota-uuid\",\"version\":\"2.0.0\",\"nonce\":777}";
    char otaId[48];
    char version[16];
    long nonce = 0;

    bool ok = iotnet::core::parseOtaTriggerPayload(
        payload,
        otaId,
        sizeof(otaId),
        version,
        sizeof(version),
        &nonce
    );

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("ota-uuid", otaId);
    TEST_ASSERT_EQUAL_STRING("2.0.0", version);
    TEST_ASSERT_EQUAL_INT(777, nonce);
}

void test_ota_update_service_build_and_consume_session() {
    iotnetesp32::ota::OtaSessionState state;
    iotnetesp32::ota::OtaTriggerData trigger = {};
    strncpy(trigger.otaId, "ota-abc", sizeof(trigger.otaId) - 1);
    strncpy(trigger.version, "1.0.9", sizeof(trigger.version) - 1);
    trigger.nonce = 99;

    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::storePendingSession(state, trigger, 100));

    char requestPayload[256];
    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::buildSessionRequestPayload(
        state,
        0x11111111,
        0x22222222,
        requestPayload,
        sizeof(requestPayload)
    ));
    TEST_ASSERT_NOT_EQUAL(0, strlen(state.correlationId()));
    TEST_ASSERT_NOT_NULL(strstr(requestPayload, "\"ota_id\":\"ota-abc\""));

    char responsePayload[256];
    snprintf(
        responsePayload,
        sizeof(responsePayload),
        "{\"cid\":\"%s\",\"session_key\":\"sess-001\",\"expires_in\":60}",
        state.correlationId()
    );

    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus status =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(state, responsePayload, &expiresIn);

    TEST_ASSERT_EQUAL(static_cast<int>(iotnetesp32::ota::SessionResponseStatus::Ready), static_cast<int>(status));
    TEST_ASSERT_EQUAL_INT(60, expiresIn);
    TEST_ASSERT_EQUAL_STRING("sess-001", state.currentSessionKey());
}

void test_json_codec_trigger_payload_invalid() {
    const char *payload = "{\"ota_id\":\"ota-uuid\",\"version\":\"2.0.0\"}";
    char otaId[48];
    char version[16];
    long nonce = 0;

    bool ok = iotnet::core::parseOtaTriggerPayload(
        payload,
        otaId,
        sizeof(otaId),
        version,
        sizeof(version),
        &nonce
    );

    TEST_ASSERT_FALSE(ok);
}

void test_ota_update_service_cid_mismatch() {
    iotnetesp32::ota::OtaSessionState state;
    iotnetesp32::ota::OtaTriggerData trigger = {};
    strncpy(trigger.otaId, "ota-abc", sizeof(trigger.otaId) - 1);
    strncpy(trigger.version, "1.0.9", sizeof(trigger.version) - 1);
    trigger.nonce = 99;
    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::storePendingSession(state, trigger, 100));

    char requestPayload[256];
    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::buildSessionRequestPayload(
        state,
        0x11111111,
        0x22222222,
        requestPayload,
        sizeof(requestPayload)
    ));

    const char *responsePayload = "{\"cid\":\"not-match\",\"session_key\":\"sess-001\",\"expires_in\":60}";
    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus status =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(state, responsePayload, &expiresIn);

    TEST_ASSERT_EQUAL(
        static_cast<int>(iotnetesp32::ota::SessionResponseStatus::CidMismatch),
        static_cast<int>(status)
    );
}

void test_ota_update_service_invalid_response_payload() {
    iotnetesp32::ota::OtaSessionState state;
    iotnetesp32::ota::OtaTriggerData trigger = {};
    strncpy(trigger.otaId, "ota-abc", sizeof(trigger.otaId) - 1);
    strncpy(trigger.version, "1.0.9", sizeof(trigger.version) - 1);
    trigger.nonce = 99;
    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::storePendingSession(state, trigger, 100));

    char requestPayload[256];
    TEST_ASSERT_TRUE(iotnetesp32::ota::OtaUpdateService::buildSessionRequestPayload(
        state,
        0x11111111,
        0x22222222,
        requestPayload,
        sizeof(requestPayload)
    ));

    const char *responsePayload = "{\"cid\":\"abc\"}";
    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus status =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(state, responsePayload, &expiresIn);

    TEST_ASSERT_EQUAL(
        static_cast<int>(iotnetesp32::ota::SessionResponseStatus::InvalidPayload),
        static_cast<int>(status)
    );
}

void test_ota_session_state_timeout() {
    iotnetesp32::ota::OtaSessionState state;
    TEST_ASSERT_TRUE(state.setPending("ota-timeout", "1.0.0", 100, 1000));

    TEST_ASSERT_FALSE(state.isTimedOut(2000, 3000));
    TEST_ASSERT_TRUE(state.isTimedOut(5000, 3000));
}

void test_ota_session_state_not_waiting() {
    iotnetesp32::ota::OtaSessionState state;

    const char *responsePayload = "{\"cid\":\"anything\",\"session_key\":\"key\",\"expires_in\":60}";
    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus status =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(state, responsePayload, &expiresIn);

    TEST_ASSERT_EQUAL(
        static_cast<int>(iotnetesp32::ota::SessionResponseStatus::CidMismatch),
        static_cast<int>(status)
    );
}

void test_json_codec_empty_payload() {
    const char *payload = "";
    char otaId[48];
    char version[16];
    long nonce = 0;

    bool ok = iotnet::core::parseOtaTriggerPayload(
        payload,
        otaId,
        sizeof(otaId),
        version,
        sizeof(version),
        &nonce
    );

    TEST_ASSERT_FALSE(ok);
}

void test_json_codec_null_payload() {
    char otaId[48];
    char version[16];
    long nonce = 0;

    bool ok = iotnet::core::parseOtaTriggerPayload(
        nullptr,
        otaId,
        sizeof(otaId),
        version,
        sizeof(version),
        &nonce
    );

    TEST_ASSERT_FALSE(ok);
}

void test_json_codec_oversized_payload() {
    char largePayload[1024];
    memset(largePayload, 'x', sizeof(largePayload) - 1);
    largePayload[sizeof(largePayload) - 1] = '\0';

    char otaId[48];
    char version[16];
    long nonce = 0;

    bool ok = iotnet::core::parseOtaTriggerPayload(
        largePayload,
        otaId,
        sizeof(otaId),
        version,
        sizeof(version),
        &nonce
    );

    TEST_ASSERT_FALSE(ok);
}

void test_ota_session_reconnect_flow() {
    iotnetesp32::ota::OtaSessionState state;

    unsigned long requestTime = 1000;
    TEST_ASSERT_TRUE(state.setPending("ota-reconnect", "1.0.0", 100, requestTime));
    TEST_ASSERT_TRUE(state.isWaiting());

    TEST_ASSERT_FALSE(state.isTimedOut(2000, 3000));

    TEST_ASSERT_TRUE(state.setCorrelationId("cid-reconnect"));
    TEST_ASSERT_TRUE(state.matchesCorrelationId("cid-reconnect"));

    char responsePayload[256];
    snprintf(responsePayload, sizeof(responsePayload),
        "{\"cid\":\"cid-reconnect\",\"session_key\":\"key-reconnect\",\"expires_in\":120}");

    int expiresIn = 0;
    iotnetesp32::ota::SessionResponseStatus status =
        iotnetesp32::ota::OtaUpdateService::consumeSessionResponse(state, responsePayload, &expiresIn);

    TEST_ASSERT_EQUAL(static_cast<int>(iotnetesp32::ota::SessionResponseStatus::Ready), static_cast<int>(status));
    TEST_ASSERT_EQUAL_INT(120, expiresIn);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_client_config_struct_initialization);
    RUN_TEST(test_client_config_null_credentials);
    RUN_TEST(test_ota_session_state_lifecycle);
    RUN_TEST(test_json_codec_trigger_payload_parse);
    RUN_TEST(test_ota_update_service_build_and_consume_session);
    RUN_TEST(test_json_codec_trigger_payload_invalid);
    RUN_TEST(test_ota_update_service_cid_mismatch);
    RUN_TEST(test_ota_update_service_invalid_response_payload);
    RUN_TEST(test_ota_session_state_timeout);
    RUN_TEST(test_ota_session_state_not_waiting);
    RUN_TEST(test_json_codec_empty_payload);
    RUN_TEST(test_json_codec_null_payload);
    RUN_TEST(test_json_codec_oversized_payload);
    RUN_TEST(test_ota_session_reconnect_flow);
    return UNITY_END();
}
