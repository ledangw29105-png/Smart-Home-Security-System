#include <Arduino.h>

#include "esp32cam.h"

#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFiClient.h>

const char *OTA_COMMAND_TOPIC = "home/system/ota";
const char *OTA_STATUS_TOPIC = "home/system/ota/status";

namespace {
PubSubClient *g_mqttClient = nullptr;
OtaTaskCallback g_onPauseTasks = nullptr;
OtaTaskCallback g_onResumeTasks = nullptr;

void publishOtaStatus(const char *stage, int progress, const String &message) {
	if (g_mqttClient == nullptr) {
		return;
	}

	String payload = "{\"stage\":\"";
	payload += stage;
	payload += "\",\"progress\":";
	payload += progress;
	payload += ",\"message\":\"";
	payload += message;
	payload += "\"}";

	g_mqttClient->publish(OTA_STATUS_TOPIC, payload.c_str(), true);
}

String extractJsonStringField(const String &json, const char *key) {
	String quotedKey = "\"";
	quotedKey += key;
	quotedKey += "\"";

	int keyPos = json.indexOf(quotedKey);
	if (keyPos < 0) {
		return "";
	}

	int colonPos = json.indexOf(':', keyPos + quotedKey.length());
	if (colonPos < 0) {
		return "";
	}

	int startQuote = json.indexOf('"', colonPos + 1);
	if (startQuote < 0) {
		return "";
	}

	int endQuote = json.indexOf('"', startQuote + 1);
	if (endQuote < 0) {
		return "";
	}

	return json.substring(startQuote + 1, endQuote);
}
} // namespace

void otaInit(PubSubClient *mqttClient) {
	g_mqttClient = mqttClient;
}

void otaSetTaskControlCallbacks(OtaTaskCallback onPause, OtaTaskCallback onResume) {
	g_onPauseTasks = onPause;
	g_onResumeTasks = onResume;
}

bool performOTA(const String &fileUrl, const String &expectedMd5) {
	if (!fileUrl.startsWith("http://") && !fileUrl.startsWith("https://")) {
		publishOtaStatus("error", 0, "URL OTA khong hop le");
		return false;
	}

	// B1: Giai phong RAM bang cach tam dung cac task nang (camera/stream/AI).
	if (g_onPauseTasks != nullptr) {
		g_onPauseTasks();
	}

	publishOtaStatus("downloading", 0, "Bat dau OTA qua HTTP");

	HTTPClient http;
	WiFiClient netClient;

	if (!http.begin(netClient, fileUrl)) {
		publishOtaStatus("error", 0, "Khong khoi tao duoc HTTPClient");
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK) {
		String err = "HTTP loi: ";
		err += httpCode;
		publishOtaStatus("error", 0, err);
		http.end();
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	int contentLength = http.getSize();
	if (contentLength <= 0) {
		publishOtaStatus("error", 0, "Khong lay duoc kich thuoc firmware");
		http.end();
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	if (!Update.begin(contentLength)) {
		String err = "Update.begin loi: ";
		err += Update.errorString();
		publishOtaStatus("error", 0, err);
		http.end();
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	// B2: Neu server gui checksum MD5 thi bat xac minh toan ven firmware.
	if (expectedMd5.length() == 32) {
		if (!Update.setMD5(expectedMd5.c_str())) {
			publishOtaStatus("error", 0, "Khong set duoc MD5");
			Update.abort();
			http.end();
			if (g_onResumeTasks != nullptr) {
				g_onResumeTasks();
			}
			return false;
		}
	}

	WiFiClient *stream = http.getStreamPtr();
	uint8_t buffer[1024];
	size_t totalWritten = 0;
	int lastProgress = -1;

	while (http.connected() && totalWritten < static_cast<size_t>(contentLength)) {
		size_t available = stream->available();
		if (available == 0) {
			delay(5);
			continue;
		}

		size_t readLen = stream->readBytes(buffer, min(available, sizeof(buffer)));
		if (readLen == 0) {
			continue;
		}

		size_t written = Update.write(buffer, readLen);
		if (written != readLen) {
			String err = "Ghi flash loi: ";
			err += Update.errorString();
			publishOtaStatus("error", lastProgress < 0 ? 0 : lastProgress, err);
			Update.abort();
			http.end();
			if (g_onResumeTasks != nullptr) {
				g_onResumeTasks();
			}
			return false;
		}

		totalWritten += written;
		int progress = static_cast<int>((totalWritten * 100) / contentLength);
		if (progress > 100) {
			progress = 100;
		}

		if (progress != lastProgress) {
			lastProgress = progress;
			publishOtaStatus("downloading", progress, "Dang cap nhat firmware");
		}
	}

	if (!Update.end(true)) {
		String err = "Update.end loi: ";
		err += Update.errorString();
		publishOtaStatus("error", lastProgress < 0 ? 0 : lastProgress, err);
		http.end();
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	if (!Update.isFinished()) {
		publishOtaStatus("error", 100, "Firmware chua ghi xong");
		http.end();
		if (g_onResumeTasks != nullptr) {
			g_onResumeTasks();
		}
		return false;
	}

	http.end();
	publishOtaStatus("success", 100, "OTA thanh cong, dang khoi dong lai");
	delay(1500);
	ESP.restart();
	return true;
}

void otaHandleMqttMessage(const char *topic, const uint8_t *payload, unsigned int length) {
	if (strcmp(topic, OTA_COMMAND_TOPIC) != 0) {
		return;
	}

	String body;
	body.reserve(length);
	for (unsigned int i = 0; i < length; ++i) {
		body += static_cast<char>(payload[i]);
	}

	String url = extractJsonStringField(body, "url");
	String md5 = extractJsonStringField(body, "md5");

	if (url.isEmpty()) {
		publishOtaStatus("error", 0, "Payload OTA thieu truong url");
		return;
	}

	performOTA(url, md5);
}
