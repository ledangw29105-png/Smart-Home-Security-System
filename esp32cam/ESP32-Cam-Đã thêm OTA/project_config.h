#pragma once

/*
 * Sao chep file nay thanh project_config.h tren may ca nhan.
 * Khong upload project_config.h co mat khau Wi-Fi that len GitHub.
 */
#define WIFI_SSID       "P303"
#define WIFI_PASSWORD   "66668888"

#define MQTT_SERVER     "test.mosquitto.org"
#define MQTT_PORT       1883

#define MOTION_TOPIC    "smart_home/ledangw29105/motion"
#define CAMERA_TOPIC    "smart_home/ledangw29105/camera"
#define COMMAND_TOPIC   "smart_home/ledangw29105/command"

#define PIR_PIN         1
#define LED_PIN         2
#define BUZZER_PIN      42

#define CAPTURE_COOLDOWN_MS 5000UL