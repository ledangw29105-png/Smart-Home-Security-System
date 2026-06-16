// Demo OTA theo kien truc MQTT trigger + HTTP pull cho ESP32-CAM.
// Luu y: File nay chi tap trung OTA, khong chua logic PIR/AI/UI.

#include <WiFi.h>
#include <PubSubClient.h>

#include "esp32cam.h"

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char *MQTT_HOST = "192.168.1.10";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_USER = "";
const char *MQTT_PASS = "";
const char *MQTT_CLIENT_ID = "esp32cam-ota-node";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void pauseHeavyTasks() {
	// TODO: Them lenh dung camera/stream tai day de tranh OOM khi OTA.
	// Vi du trong du an that: stop streaming task, esp_camera_deinit(), giai phong PSRAM.
}

void resumeHeavyTasks() {
	// TODO: Neu OTA that bai va khong restart, khoi phuc lai camera/stream tai day.
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
	// Ghep OTA vao callback MQTT hien tai cua nhom:
	// 1) Giu nguyen cac nhanh topic khac
	// 2) Them dong otaHandleMqttMessage(...) de bat topic home/system/ota
	otaHandleMqttMessage(topic, payload, length);
}

void connectWiFi() {
	if (WiFi.status() == WL_CONNECTED) {
		return;
	}

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}
}

void connectMqtt() {
	while (!mqttClient.connected()) {
		bool ok = false;
		if (strlen(MQTT_USER) == 0) {
			ok = mqttClient.connect(MQTT_CLIENT_ID);
		} else {
			ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
		}

		if (ok) {
			mqttClient.subscribe(OTA_COMMAND_TOPIC);
			mqttClient.publish(OTA_STATUS_TOPIC, "{\"stage\":\"idle\",\"progress\":0,\"message\":\"OTA san sang\"}", true);
		} else {
			delay(1500);
		}
	}
}

void setup() {
	Serial.begin(115200);

	connectWiFi();

	mqttClient.setServer(MQTT_HOST, MQTT_PORT);
	mqttClient.setCallback(mqttCallback);

	otaInit(&mqttClient);
	otaSetTaskControlCallbacks(pauseHeavyTasks, resumeHeavyTasks);

	connectMqtt();
}

void loop() {
	if (WiFi.status() != WL_CONNECTED) {
		connectWiFi();
	}

	if (!mqttClient.connected()) {
		connectMqtt();
	}

	mqttClient.loop();
}

