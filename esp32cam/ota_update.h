#pragma once

#include <Arduino.h>
#include <stdint.h>

class PubSubClient;

extern const char *OTA_COMMAND_TOPIC;
extern const char *OTA_STATUS_TOPIC;

typedef void (*OtaTaskCallback)();

void otaInit(PubSubClient *mqttClient);
void otaSetTaskControlCallbacks(OtaTaskCallback onPause, OtaTaskCallback onResume);
void otaHandleMqttMessage(const char *topic, const uint8_t *payload, unsigned int length);
bool performOTA(const String &fileUrl, const String &expectedMd5 = "");