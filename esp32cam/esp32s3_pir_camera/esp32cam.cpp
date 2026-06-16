#include "esp32cam.h"

#include <WiFi.h>
#include <PubSubClient.h>

#include "board_config.h"
#include "project_config.h"

void startCameraServer();
void setupLedFlash();

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static bool ledState = false;
static bool buzzerState = false;
static String receivedCommand;

void initHardware() {
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  ledState = false;
  buzzerState = false;

  Serial.println("Da khoi tao PIR, LED va buzzer");
}

bool initCamera() {
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("Khoi tao camera that bai, ma loi: 0x%x\n", error);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    Serial.println("Khong lay duoc camera sensor");
    return false;
  }

  if (sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
  }

  sensor->set_framesize(sensor, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  sensor->set_vflip(sensor, 1);
  sensor->set_hmirror(sensor, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  sensor->set_vflip(sensor, 1);
#endif

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  Serial.println("Khoi tao camera thanh cong");
  return true;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);

  Serial.print("Dang ket noi WiFi");
  const unsigned long startTime = millis();
  const unsigned long timeoutMs = 30000UL;

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= timeoutMs) {
      Serial.println();
      Serial.println("Ket noi WiFi that bai");
      return false;
    }

    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Ket noi WiFi thanh cong");
  Serial.print("Dia chi IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectMQTT() {
  if (mqttClient.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Khong the ket noi MQTT vi WiFi dang mat");
    return false;
  }

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(receiveCommand);
  mqttClient.setBufferSize(512);

  const uint64_t chipId = ESP.getEfuseMac();
  char clientId[50];
  snprintf(
    clientId,
    sizeof(clientId),
    "ESP32S3_CAM_%08X%08X",
    (uint32_t)(chipId >> 32),
    (uint32_t)chipId
  );

  Serial.print("Dang ket noi MQTT...");

  if (!mqttClient.connect(clientId)) {
    Serial.print("THAT BAI, ma loi = ");
    Serial.println(mqttClient.state());
    return false;
  }

  Serial.println("THANH CONG");

  if (mqttClient.subscribe(COMMAND_TOPIC)) {
    Serial.println("Da subscribe topic dieu khien");
  } else {
    Serial.println("Subscribe topic dieu khien that bai");
  }

  const bool sent = mqttClient.publish(
    MOTION_TOPIC,
    "{\"device\":\"esp32s3_cam\",\"status\":\"online\"}",
    true
  );

  Serial.println(sent ? "Da gui trang thai online" : "Gui trang thai online that bai");
  return true;
}

void reconnectMQTT() {
  static unsigned long lastReconnectTime = 0;
  const unsigned long reconnectIntervalMs = 5000UL;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    if (millis() - lastReconnectTime >= reconnectIntervalMs) {
      lastReconnectTime = millis();
      connectMQTT();
    }
    return;
  }

  mqttClient.loop();
}

bool detectMotion() {
  return digitalRead(PIR_PIN) == HIGH;
}

camera_fb_t *captureImage() {
  Serial.println("DANG CHUP ANH...");

  camera_fb_t *frameBuffer = esp_camera_fb_get();
  if (frameBuffer == nullptr) {
    Serial.println("LOI: KHONG CHUP DUOC ANH");
    return nullptr;
  }

  Serial.printf(
    "CHUP ANH THANH CONG - KICH THUOC: %u BYTES\n",
    (unsigned int)frameBuffer->len
  );

  Serial.printf(
    "DO PHAN GIAI: %d x %d\n",
    frameBuffer->width,
    frameBuffer->height
  );

  return frameBuffer;
}

bool sendImage(camera_fb_t *frameBuffer) {
  if (frameBuffer == nullptr) {
    return false;
  }

  if (!mqttClient.connected()) {
    Serial.println("Khong gui duoc thong tin anh vi MQTT dang mat");
    return false;
  }

  const String imageUrl = "http://" + WiFi.localIP().toString() + "/capture";
  char cameraMessage[300];

  snprintf(
    cameraMessage,
    sizeof(cameraMessage),
    "{\"captured\":true,\"size\":%u,\"width\":%d,\"height\":%d,\"image_url\":\"%s\"}",
    (unsigned int)frameBuffer->len,
    frameBuffer->width,
    frameBuffer->height,
    imageUrl.c_str()
  );

  const bool sent = mqttClient.publish(CAMERA_TOPIC, cameraMessage);

  if (sent) {
    Serial.println("Da gui thong tin anh qua MQTT");
    Serial.print("Dia chi anh: ");
    Serial.println(imageUrl);
  } else {
    Serial.println("Gui thong tin anh that bai");
  }

  return sent;
}

void publishMotionStatus(bool status) {
  if (!mqttClient.connected()) {
    Serial.println("Khong gui duoc trang thai PIR vi MQTT dang mat");
    return;
  }

  const char *message = status ? "{\"motion\":true}" : "{\"motion\":false}";
  const bool sent = mqttClient.publish(MOTION_TOPIC, message);

  if (sent) {
    Serial.print("Da gui trang thai chuyen dong: ");
    Serial.println(status ? "true" : "false");
  } else {
    Serial.println("Gui trang thai chuyen dong that bai");
  }
}

void turnOnLED() {
  digitalWrite(LED_PIN, HIGH);
  ledState = true;
  Serial.println("LED DA BAT");
}

void turnOffLED() {
  digitalWrite(LED_PIN, LOW);
  ledState = false;
  Serial.println("LED DA TAT");
}

void turnOnBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerState = true;
  Serial.println("BUZZER DA BAT");
}

void turnOffBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);
  buzzerState = false;
  Serial.println("BUZZER DA TAT");
}

void receiveCommand(char *topic, byte *payload, unsigned int length) {
  receivedCommand = "";

  for (unsigned int i = 0; i < length; i++) {
    receivedCommand += (char)payload[i];
  }

  receivedCommand.trim();

  Serial.print("Nhan MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(receivedCommand);

  executeCommand(receivedCommand);
}

void executeCommand(const String &command) {
  if (command == "LED_ON") {
    turnOnLED();
  } else if (command == "LED_OFF") {
    turnOffLED();
  } else if (command == "BUZZER_ON") {
    turnOnBuzzer();
  } else if (command == "BUZZER_OFF") {
    turnOffBuzzer();
  } else if (command == "ALARM_ON") {
    turnOnLED();
    turnOnBuzzer();
  } else if (command == "ALARM_OFF") {
    turnOffLED();
    turnOffBuzzer();
  } else if (command == "TEST") {
    Serial.println("ESP32 DA NHAN DUOC LENH TEST");
  } else {
    Serial.println("LENH KHONG HOP LE");
  }
}

void startCameraWebServer() {
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}
