#include "esp32cam.h"
#include "project_config.h"

#include "esp_sleep.h"
#include "driver/rtc_io.h"

// Trạng thái PIR trước đó
bool previousMotionState = false;

// Thời điểm chụp ảnh gần nhất
unsigned long lastCaptureTime = 0;

// Thời điểm PIR bắt đầu về LOW
unsigned long noMotionStartTime = 0;

// PIR phải LOW liên tục 5 giây mới đi ngủ
const unsigned long NO_MOTION_SLEEP_MS = 5000UL;

// GPIO1 đang nối với OUT của PIR
static const gpio_num_t PIR_WAKE_GPIO = GPIO_NUM_1;


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("===== SMART HOME SECURITY =====");

  /*
   * Kiểm tra nguyên nhân ESP32 khởi động.
   */
  const esp_sleep_wakeup_cause_t wakeupCause =
      esp_sleep_get_wakeup_cause();

  if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0) {
    /*
     * Sau khi thức bởi Deep Sleep,
     * GPIO1 đang ở chế độ RTC GPIO.
     * Phải chuyển lại thành GPIO bình thường.
     */
    rtc_gpio_deinit(PIR_WAKE_GPIO);

    Serial.println("THUC DAY BOI PIR");
  } else {
    Serial.println("KHOI DONG BINH THUONG");
  }

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

  if (!connectMQTT()) {
    Serial.println("MQTT KET NOI THAT BAI");
  }

  startCameraWebServer();

  /*
   * Đặt trạng thái ban đầu là false để:
   * nếu ESP32 vừa được PIR đánh thức và PIR đang HIGH,
   * vòng loop đầu tiên sẽ gửi motion:true và chụp ngay.
   */
  previousMotionState = false;
  lastCaptureTime = 0;
  noMotionStartTime = 0;

  Serial.println("BAT DAU THEO DOI PIR");
}


void loop() {
  // Duy trì Wi-Fi và MQTT
  reconnectMQTT();

  const bool motionDetected = detectMotion();
  const unsigned long currentTime = millis();
  static unsigned long lastPirPrintTime = 0;

if (currentTime - lastPirPrintTime >= 1000UL) {
  lastPirPrintTime = currentTime;

  Serial.print("TRANG THAI PIR: ");
  Serial.println(motionDetected ? "HIGH" : "LOW");
}

  /*
   * ================= PIR HIGH =================
   */
  if (motionDetected) {
    // Có chuyển động trở lại thì hủy bộ đếm đi ngủ
    noMotionStartTime = 0;

    /*
     * Chỉ gửi motion:true khi PIR vừa chuyển:
     * LOW -> HIGH
     */
    if (!previousMotionState) {
      Serial.println();
      Serial.println("PHAT HIEN CHUYEN DONG");

      publishMotionStatus(true);

      // Cho MQTT xử lý bản tin motion:true
      reconnectMQTT();
      delay(50);
    }

    /*
     * Chụp ngay ảnh đầu tiên.
     * Nếu PIR vẫn HIGH thì cứ sau 5 giây chụp tiếp.
     *
     * CAPTURE_COOLDOWN_MS phải đặt là 5000UL
     * trong project_config.h.
     */
    if (
      lastCaptureTime == 0 ||
      currentTime - lastCaptureTime >= CAPTURE_COOLDOWN_MS
    ) {
      Serial.println("BAT DAU CHUP ANH");

      camera_fb_t *frameBuffer = captureImage();

      if (frameBuffer != nullptr) {
        Serial.print("CHUP ANH THANH CONG - KICH THUOC: ");
        Serial.print(frameBuffer->len);
        Serial.println(" bytes");

        const bool sendResult = sendImage(frameBuffer);

        if (sendResult) {
          Serial.println("DA GUI THONG TIN ANH LEN MQTT");
        } else {
          Serial.println("GUI THONG TIN ANH LEN MQTT THAT BAI");
        }

        // Trả bộ nhớ ảnh lại cho camera
        esp_camera_fb_return(frameBuffer);
      } else {
        Serial.println("CHUP ANH THAT BAI");
      }

      /*
       * Ghi thời gian sau khi quá trình chụp và gửi hoàn thành.
       */
      lastCaptureTime = millis();

      // Cho MQTT xử lý dữ liệu vừa gửi
      reconnectMQTT();
      delay(50);
    }
  }

  /*
   * ================= PIR LOW =================
   */
  else {
    /*
     * PIR vừa chuyển:
     * HIGH -> LOW
     */
    if (previousMotionState) {
      Serial.println();
      Serial.println("PIR DA VE LOW");
      Serial.println("BAT DAU DEM 5 GIAY KHONG CO CHUYEN DONG");

      noMotionStartTime = currentTime;
    }

    /*
     * Trường hợp ESP32 vừa khởi động nhưng PIR đang LOW.
     */
    if (noMotionStartTime == 0) {
      noMotionStartTime = currentTime;
    }

    /*
     * Nếu PIR LOW liên tục đủ 5 giây:
     * - gửi motion:false;
     * - tắt LED và buzzer;
     * - chờ MQTT gửi xong;
     * - vào Deep Sleep.
     */
    if (
      currentTime - noMotionStartTime >= NO_MOTION_SLEEP_MS
    ) {
      Serial.println();
      Serial.println("KHONG CON CHUYEN DONG");

      publishMotionStatus(false);

      turnOffLED();
      turnOffBuzzer();

      /*
       * Giữ MQTT hoạt động thêm 1,5 giây
       * để bản tin motion:false được gửi đi.
       */
      const unsigned long mqttWaitStart = millis();

      while (millis() - mqttWaitStart < 1500UL) {
        reconnectMQTT();
        delay(10);
      }

      Serial.println("DA GUI MOTION FALSE LEN MQTT");
      Serial.println("DA TAT LED VA BUZZER");

      /*
       * Đánh thức khi GPIO1 lên HIGH.
       */
      const esp_err_t wakeResult =
          esp_sleep_enable_ext0_wakeup(PIR_WAKE_GPIO, 1);

      if (wakeResult != ESP_OK) {
        Serial.print("CAU HINH WAKEUP THAT BAI, MA LOI: ");
        Serial.println(wakeResult);

        delay(1000);
        return;
      }

      /*
       * Giải phóng camera trước khi ngủ.
       */
      esp_camera_deinit();

      Serial.println("ESP32 DANG NGU");
      Serial.println("CHO PIR PHAT HIEN NGUOI...");
      Serial.flush();

      delay(100);

      /*
       * ESP32 ngủ tại đây.
       * Khi PIR lên HIGH, ESP32 thức và chạy lại setup().
       */
      esp_deep_sleep_start();
    }
  }

  previousMotionState = motionDetected;

  delay(100);
}