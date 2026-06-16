# Cách dùng bộ code đã tách hàm

Đặt toàn bộ file vào cùng một thư mục Arduino sketch:

```text
esp32s3_pir_camera/
├── esp32s3_pir_camera.ino
├── esp32cam.h
├── esp32cam.cpp
├── project_config.h
├── app_httpd.cpp
├── board_config.h
├── camera_index.h
└── camera_pins.h
```

Bốn file camera lấy từ sketch CameraWebServer đang chạy được:

- app_httpd.cpp
- board_config.h
- camera_index.h
- camera_pins.h

Sao chép `project_config.h.example` thành `project_config.h`, rồi điền Wi-Fi thật.
Không upload `project_config.h` lên GitHub.

Thiết lập Arduino IDE:

- Board: ESP32S3 Dev Module
- USB CDC On Boot: Enabled
- Flash Size: 16MB
- PSRAM: OPI PSRAM
- Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
- Upload Mode: UART0 / Hardware CDC

MQTTBox subscribe:

```text
smart_home/ledangw29105/#
```

Các lệnh:

```text
TEST
LED_ON
LED_OFF
BUZZER_ON
BUZZER_OFF
ALARM_ON
ALARM_OFF
```
