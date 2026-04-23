#include "ota/FirmwareFlasher.h"

#include <HTTPClient.h>
#include <Update.h>

namespace iotnetesp32::ota {

bool FirmwareFlasher::downloadAndFlash(const char *url) {
    if (!url || strlen(url) == 0) {
        Serial.println("[OTA-DOWNLOAD] FAIL: Invalid download URL");
        return false;
    }

    Serial.printf("[OTA-DOWNLOAD] Starting download: %s\n", url);

    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(30000);

    int httpCode = http.begin(url);
    if (httpCode <= 0) {
        Serial.printf("[OTA-DOWNLOAD] FAIL: HTTP begin error: %d\n", httpCode);
        http.end();
        return false;
    }

    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA-DOWNLOAD] FAIL: HTTP GET returned %d\n", httpCode);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    int contentLength = http.getSize();

    if (contentLength <= 0) {
        Serial.println("[OTA-DOWNLOAD] FAIL: Invalid content length");
        http.end();
        return false;
    }

    Serial.printf("[OTA-DOWNLOAD] Size: %d bytes, heap: %d\n", contentLength, ESP.getFreeHeap());

    if (!Update.begin(contentLength)) {
        Serial.printf("[OTA-DOWNLOAD] FAIL: Update.begin: %s\n", Update.errorString());
        http.end();
        return false;
    }

    size_t written = Update.writeStream(*stream);
    if (written != contentLength) {
        Serial.printf(
            "[OTA-DOWNLOAD] FAIL: Write mismatch: expected %d, got %zu\n",
            contentLength,
            written
        );
        Update.abort();
        http.end();
        return false;
    }

    Serial.printf("[OTA-DOWNLOAD] Written: %zu bytes\n", written);

    if (!Update.end()) {
        Serial.printf("[OTA-DOWNLOAD] FAIL: Update.end: %s\n", Update.errorString());
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        Serial.println("[OTA-DOWNLOAD] FAIL: Update verification failed");
        http.end();
        return false;
    }

    Serial.println("[OTA-DOWNLOAD] OK: Firmware flashed successfully");
    http.end();
    return true;
}

}
