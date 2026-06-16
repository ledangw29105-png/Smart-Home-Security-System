#include "esp32cam.h"
#include "project_config.h"

bool previousMotionState = false;
unsigned long lastCaptureTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.setDebugOutput(true);
  Serial.println();

  initHardware();

  if (!initCamera()) {
    Serial.println("Dung chuong trinh vi camera bi loi");
    while (true) {
      delay(1000);
    }
  }

  if (!connectWiFi()) {
    Serial.println("Dung chuong trinh vi WiFi bi loi");
    while (true) {
      delay(1000);
    }
  }

  connectMQTT();
  startCameraWebServer();
}

void loop() {
  reconnectMQTT();

  const bool motionDetected = detectMotion();

  if (
    motionDetected &&
    (
      lastCaptureTime == 0 ||
      millis() - lastCaptureTime >= CAPTURE_COOLDOWN_MS
    )
  ) {
    Serial.println("PHAT HIEN CHUYEN DONG");

    publishMotionStatus(true);

    camera_fb_t *frameBuffer = captureImage();

    if (frameBuffer != nullptr) {
      sendImage(frameBuffer);
      esp_camera_fb_return(frameBuffer);
    }

    lastCaptureTime = millis();
  }

  if (!motionDetected && previousMotionState) {
    Serial.println("KHONG CON CHUYEN DONG");
    publishMotionStatus(false);
  }

  previousMotionState = motionDetected;
  delay(100);
}
