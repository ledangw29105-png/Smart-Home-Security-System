

kiến trúc `MQTT trigger + HTTP pull`.

## 1. Mục tiêu của phần OTA

Luồng hoạt động: lưu file upload file .bin từ dashboard sau đó lưu tại thư mục uploads
                 tiếp túc gửi url xuống để esp32 tải file qua http
                 tải và cập nhật firmware

1. Server lưu sẵn file `firmware.bin`.
2. Server publish MQTT xuống ESP32-CAM với payload chứa URL firmware.
3. ESP32-CAM nhận lệnh, tạm dừng các tác vụ nặng để tránh OOM.
4. ESP32-CAM tải file bằng `HTTPClient`.
5. ESP32-CAM ghi firmware bằng `Update.h`.
6. ESP32-CAM publish tiến độ và lỗi qua MQTT.
7. ESP32-CAM reboot sau khi cập nhật thành công.


## 2. Các file đã viết

### 2.1. `esp32cam/esp32cam.h`

- `otaInit(PubSubClient *mqttClient)` để gắn MQTT client vào module OTA.
- `otaSetTaskControlCallbacks(...)` để truyền callback dừng/mở lại task nặng.
- `otaHandleMqttMessage(...)` để xử lý payload MQTT khi nhận lệnh OTA.
- `performOTA(...)` là hàm chính tải firmware và ghi flash.

### 2.2. `esp32cam/esp32cam.cpp`

Các hằng topic:

- `home/system/ota` là topic nhận lệnh OTA.
- `home/system/ota/status` là topic báo trạng thái.

### 2.3. `ota_control/mock_ota_server.js`

- Nhận file qua API `POST /api/ota/upload`.
- Lưu file thành `firmware.bin` trong thư mục `uploads`.
- Tạo URL tĩnh cho file bin.
- Tính MD5 để gửi kèm payload MQTT.

### 2.4. `platformio.ini`

File cấu hình chung cho project ESP32:

- Cho phép build từ thư mục `main`.
- Thêm `esp32cam` vào include path.
- Kéo dependency chung `PubSubClient` từ PlatformIO.

## 3. Giải thích code ESP32

### 3.1. `otaInit(PubSubClient *mqttClient)`
Hàm này gán MQTT client hiện tại vào module OTA.
Mục đích:
- Cho phép `performOTA()` publish tiến độ và lỗi.
- Không cần tạo MQTT client riêng trong module OTA.

### 3.2. `otaSetTaskControlCallbacks(...)`

Hàm này nhận 2 callback:

- callback dừng tác vụ nặng trước OTA.
- callback khôi phục tác vụ sau OTA nếu có lỗi.

Trong project ESP32-CAM, phần này thường dùng để:

- dừng stream camera.
- deinit camera.
- giải phóng buffer/PSRAM nếu có.

### 3.3. `otaHandleMqttMessage(...)`

Hàm này chỉ xử lý topic `home/system/ota`.

Nó:

- đọc JSON payload từ MQTT.
- lấy trường `url`.
- lấy trường `md5` nếu có.
- gọi `performOTA(url, md5)`.

Payload mẫu:

```json
{"url":"http://localhost:8081/files/firmware.bin","md5":"19063ad6513319c3886a1d9df4154804"}
```

### 3.4. `performOTA(const String &fileUrl, const String &expectedMd5)`

Đây là hàm chính của OTA.

Luồng xử lý:

1. Kiểm tra URL hợp lệ.
2. Gọi callback dừng task nặng.
3. Khởi tạo HTTP client.
4. Gửi `GET` để tải firmware.
5. Kiểm tra HTTP status.
6. Lấy `content length`.
7. Gọi `Update.begin(contentLength)`.
8. Nếu có MD5 thì gọi `Update.setMD5(...)`.
9. Đọc dữ liệu theo từng khối và ghi vào flash bằng `Update.write(...)`.
10. Publish tiến độ qua MQTT.
11. Gọi `Update.end(true)`.
12. Nếu thành công thì reboot.

### 3.5. Publish tiến độ

Module OTA publish JSON lên `home/system/ota/status` với format:

```json
{"stage":"downloading","progress":45,"message":"Dang cap nhat firmware"}
```

Các stage thường dùng:

- `idle`
- `downloading`
- `success`
- `error`

## 4. Cách ghép vào module của nhóm

### 4.1. Thành viên làm MQTT / Dashboard

Nhiệm vụ của bạn là publish lệnh OTA đúng topic.

Topic:

- `home/system/ota`

Payload:

```json
{"url":"http://<ip-may-chay-server>:8081/files/firmware.bin","md5":"<md5>"}
```

Gợi ý:

- Nếu test nội bộ trên PC thì có thể dùng `localhost`.
- Nếu ESP32-CAM thật chạy OTA qua Wi-Fi thì phải dùng IP LAN của máy chạy server, không dùng `localhost`.

### 4.2. Thành viên làm camera / phần cứng

Trước khi OTA, cần dừng các task nặng.

Trong code hiện tại có 2 callback để gắn vào:

- `pauseHeavyTasks()`
- `resumeHeavyTasks()`

Gợi ý cách triển khai:

```cpp
void pauseHeavyTasks() {
	// Dung camera stream
	// esp_camera_deinit();
	// Stop task dang dung RAM lon
}

void resumeHeavyTasks() {
	// Khoi phuc camera neu OTA that bai va khong reboot
}
```

### 4.3. Thành viên làm AI xử lý ảnh

Không cần sửa logic OTA.

Chỉ cần đảm bảo module AI có thể được pause khi OTA bắt đầu.

Nếu AI đang giữ buffer lớn, nên:

- dừng vòng lặp xử lý ảnh.
- giải phóng frame buffer.
- tránh vừa nhận ảnh vừa OTA.

### 4.4. Thành viên làm main firmware

Trong callback MQTT hiện tại, thêm 1 dòng gọi OTA handler.

Ví dụ:

```cpp
void mqttCallback(char *topic, byte *payload, unsigned int length) {
	otaHandleMqttMessage(topic, payload, length);
}
```

Trong `setup()`:

```cpp
otaInit(&mqttClient);
otaSetTaskControlCallbacks(pauseHeavyTasks, resumeHeavyTasks);
mqttClient.subscribe(OTA_COMMAND_TOPIC);
```

## 5. Code mock server để test upload

Server dùng file [ota_control/mock_ota_server.js](ota_control/mock_ota_server.js).

Luồng upload:

1. Gửi file `.bin` vào `POST /api/ota/upload`.
2. Server lưu file thành `firmware.bin`.
3. Server tính MD5.
4. Server trả về URL tĩnh và MD5.

Response mẫu:

```json
{
	"message": "Upload firmware thanh cong",
	"url": "http://192.168.1.238:8081/files/firmware.bin",
	"md5": "19063ad6513319c3886a1d9df4154804"
}
```

## 6. Cách test phần upload

### 6.1. Chạy server

Nếu port mặc định bị chiếm, đổi port khác:

```powershell
$env:PORT=8081; npm run start:ota-server
```

### 6.2. Upload file thử

Gửi file `.bin` bằng Postman hoặc curl:

```powershell
curl.exe -X POST -F "firmware=@C:\path\to\firmware.bin" http://localhost:8081/api/ota/upload
```

### 6.3. Kiểm tra kết quả

Nếu thành công, server trả về:

- `url`
- `md5`

Sau đó có thể lấy URL đó để publish MQTT OTA.

## 7. Cách test toàn bộ OTA với ESP32-CAM

1. Chạy server trên máy tính.
2. Upload firmware và lấy `url` + `md5`.
3. ESP32-CAM phải kết nối được tới URL đó qua LAN.
4. Publish MQTT vào topic `home/system/ota`.
5. Theo dõi status ở topic `home/system/ota/status`.
6. Nếu thành công, ESP32-CAM sẽ reboot.

## 8. Lưu ý quan trọng

- `localhost` chỉ dùng để test trên máy đang chạy server.
- ESP32-CAM thật phải dùng IP LAN của máy server.
- Trước OTA phải tạm dừng camera để tránh thiếu RAM.
- MD5 nên được truyền kèm để kiểm tra toàn vẹn firmware.
- Không truyền file lớn qua MQTT.

## 9. Checklist cho nhóm

- MQTT có topic `home/system/ota`.
- ESP32-CAM subscribe topic OTA.
- Có callback dừng camera trước OTA.
- Server trả được `firmware.bin` qua HTTP.
- Payload có `url` và `md5`.
- ESP32-CAM publish được trạng thái OTA.
- Thiết bị reboot sau khi update thành công.


