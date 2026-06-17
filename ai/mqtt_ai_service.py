import os
import json
import urllib.request
from datetime import datetime

import paho.mqtt.client as mqtt

from image_processor import process_image
from risk_evaluator import evaluate_risk


# =========================
# PATH CONFIG
# =========================

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CAPTURE_DIR = os.path.join(BASE_DIR, "captured_images")


# =========================
# MQTT CONFIG
# =========================

# Dùng MQTT Broker public để mô phỏng cloud
MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883

# ESP32-CAM publish thông tin ảnh lên topic này
IMAGE_TOPIC = "smart_home/ledangw29105/camera"

# AI publish kết quả nhận diện lên topic này cho dashboard
AI_RESULT_TOPIC = "smart_home/ledangw29105/ai_result"

# AI gửi lệnh điều khiển về ESP32-CAM qua topic command
COMMAND_TOPIC = "smart_home/ledangw29105/command"


# =========================
# IMAGE FUNCTIONS
# =========================

def parse_camera_message(payload):
    """
    Nhận payload MQTT dạng JSON từ ESP32-CAM.

    ESP32 gửi dạng ví dụ:
    {
        "captured": true,
        "size": 12345,
        "width": 320,
        "height": 240,
        "image_url": "http://192.168.x.x/capture"
    }
    """

    message = payload.decode("utf-8")
    data = json.loads(message)

    if "image_url" not in data:
        raise ValueError("Payload MQTT không có trường image_url")

    return data


def download_image_from_url(image_url):
    """
    Tải ảnh từ ESP32-CAM thông qua URL /capture
    rồi lưu vào thư mục ai/captured_images.
    """

    os.makedirs(CAPTURE_DIR, exist_ok=True)

    file_name = datetime.now().strftime("esp32_%Y%m%d_%H%M%S.jpg")
    image_path = os.path.join(CAPTURE_DIR, file_name)

    print("Đang tải ảnh từ ESP32-CAM:", image_url)

    with urllib.request.urlopen(image_url, timeout=10) as response:
        image_data = response.read()

    if len(image_data) == 0:
        raise ValueError("Ảnh tải về bị rỗng")

    with open(image_path, "wb") as f:
        f.write(image_data)

    return image_path


# =========================
# MQTT CALLBACKS
# =========================

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Đã kết nối MQTT Broker:", MQTT_BROKER)
        client.subscribe(IMAGE_TOPIC)
        print("Đang lắng nghe topic:", IMAGE_TOPIC)
    else:
        print("Kết nối MQTT thất bại, mã lỗi:", rc)


def on_disconnect(client, userdata, rc):
    print("Mất kết nối MQTT, mã lỗi:", rc)


def publish_ai_result(client, result):
    """
    Gửi kết quả AI lên MQTT cho dashboard.
    """

    payload = json.dumps(result, ensure_ascii=False)

    client.publish(
        AI_RESULT_TOPIC,
        payload,
        retain=False
    )

    print("Đã publish kết quả AI lên:", AI_RESULT_TOPIC)


def publish_alarm_command(client, danger_level):
    """
    Gửi lệnh điều khiển dạng text về ESP32-CAM.

    ESP32 hiểu các lệnh:
    - ALARM_ON
    - ALARM_OFF
    """

    if danger_level >= 3:
        command = "ALARM_ON"
    else:
        command = "ALARM_OFF"

    client.publish(
        COMMAND_TOPIC,
        command,
        retain=False
    )

    print("Đã gửi lệnh về ESP32:", command)


def on_message(client, userdata, msg):
    print("\n==============================")
    print("Nhận dữ liệu từ topic:", msg.topic)

    if msg.topic != IMAGE_TOPIC:
        print("Topic không đúng, bỏ qua")
        return

    try:
        # 1. Đọc JSON từ ESP32-CAM
        camera_data = parse_camera_message(msg.payload)
        print("Dữ liệu camera:", camera_data)

        # 2. Lấy image_url
        image_url = camera_data["image_url"]

        # 3. Tải ảnh từ ESP32-CAM
        image_path = download_image_from_url(image_url)
        print("Đã tải ảnh:", image_path)

        # 4. Gọi AI xử lý ảnh
        result = process_image(image_path)
        print("Kết quả AI:", result)

        # 5. Lưu thêm thông tin ảnh gốc vào kết quả
        result["camera_data"] = camera_data
        result["image_url"] = image_url
        result["captured_image_path"] = image_path

        # 6. Đánh giá mức độ nguy hiểm
        risk = evaluate_risk(
            ai_result=result,
            motion_detected=True,
            motion_duration=0
        )

        result["danger_level"] = risk["danger_level"]
        result["risk_label"] = risk["risk_label"]
        result["risk_message"] = risk["message"]
        result["alarm"] = risk["alarm"]

        print("Mức độ nguy hiểm:", risk)

        # 7. Gửi kết quả AI cho dashboard
        publish_ai_result(client, result)

        # 8. Gửi lệnh ALARM_ON / ALARM_OFF về ESP32
        publish_alarm_command(client, result["danger_level"])

    except Exception as e:
        print("Lỗi xử lý ảnh:", e)

        error_result = {
            "status": "error",
            "message": str(e),
            "alert": False,
            "people": [],
            "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }

        publish_ai_result(client, error_result)

        # Khi lỗi thì tạm thời không bật còi để tránh báo động sai
        client.publish(COMMAND_TOPIC, "ALARM_OFF", retain=False)
        print("Đã gửi lệnh về ESP32: ALARM_OFF")


# =========================
# MAIN
# =========================

def create_mqtt_client():
    """
    Tạo MQTT client.
    Dùng CallbackAPIVersion.VERSION1 để tránh warning của paho-mqtt bản mới.
    Nếu máy dùng bản cũ thì tự fallback về mqtt.Client().
    """

    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    except AttributeError:
        client = mqtt.Client()

    return client


def main():
    client = create_mqtt_client()

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    print("Đang kết nối MQTT Broker...")
    print("Broker:", MQTT_BROKER)
    print("Port:", MQTT_PORT)

    client.connect(MQTT_BROKER, MQTT_PORT, 60)

    print("Đang chạy AI MQTT Service...")
    client.loop_forever()


if __name__ == "__main__":
    main()