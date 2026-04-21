#include "IotNetESP32.h"

unsigned long IotNetESP32::getExecutionTime() {
    if (endTimestamp > 0 && startTimestamp > 0) {
        return endTimestamp - startTimestamp;
    } else if (timingActive && startTimestamp > 0) {
        return millis() - startTimestamp;
    }
    return 0;
}

String IotNetESP32::getFormattedExecutionTime() {
    unsigned long totalTime = getExecutionTime();
    if (totalTime == 0) {
        return "0ms";
    }

    String formattedTime;

    unsigned long hours = totalTime / 3600000;
    unsigned long minutes = (totalTime % 3600000) / 60000;
    unsigned long seconds = (totalTime % 60000) / 1000;
    unsigned long milliseconds = totalTime % 1000;

    if (hours > 0) {
        formattedTime += String(hours) + "h ";
    }

    if (minutes > 0 || hours > 0) {
        formattedTime += String(minutes) + "m ";
    }

    if (seconds > 0 || minutes > 0 || hours > 0) {
        formattedTime += String(seconds) + "s ";
    }

    formattedTime += String(milliseconds) + "ms";

    return formattedTime;
}

void IotNetESP32::configureTime(
    const char *timezone,
    const char *ntpServer1,
    const char *ntpServer2,
    const char *ntpServer3
) {
    if (!timezone || !ntpServer1) {
        Serial.println("Error: Invalid time configuration parameters");
        return;
    }

    strncpy(timeZone, timezone, sizeof(timeZone) - 1);
    timeZone[sizeof(timeZone) - 1] = '\0';
    configTzTime(timezone, ntpServer1, ntpServer2, ntpServer3);

    int retry = 0;
    const int maxRetries = 10;

    Serial.print("Waiting for NTP time sync");
    while (!isTimeSet() && retry < maxRetries) {
        Serial.print(".");
        delay(500);
        retry++;
    }
    Serial.println("");

    if (isTimeSet()) {
        timeConfigured = true;
        Serial.println("Time synchronized with NTP server");
        Serial.printf("Current time: %s\n", getFormattedTime().c_str());
    } else {
        Serial.println("Failed to synchronize time with NTP server");
    }
}

bool IotNetESP32::isTimeSet() {
    time_t now = time(nullptr);
    return now > 1600000000;
}

String IotNetESP32::getFormattedTime(const char *format) {
    if (!isTimeSet()) {
        return "Time not synchronized";
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
    char buffer[80];

    localtime_r(&now, &timeinfo);
    strftime(buffer, sizeof(buffer), format, &timeinfo);

    return String(buffer);
}
