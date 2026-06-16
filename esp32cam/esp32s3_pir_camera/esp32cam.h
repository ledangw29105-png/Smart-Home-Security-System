#pragma once

#include <Arduino.h>
#include "esp_camera.h"

void initHardware();
bool initCamera();
bool connectWiFi();
bool connectMQTT();
void reconnectMQTT();

bool detectMotion();
camera_fb_t *captureImage();
bool sendImage(camera_fb_t *frameBuffer);
void publishMotionStatus(bool status);

void turnOnBuzzer();
void turnOffBuzzer();
void turnOnLED();
void turnOffLED();

void receiveCommand(char *topic, byte *payload, unsigned int length);
void executeCommand(const String &command);

void startCameraWebServer();
