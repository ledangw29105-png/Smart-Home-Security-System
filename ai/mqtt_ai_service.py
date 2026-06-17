import os
import json
from datetime import datetime

import paho.mqtt.client as mqtt

from image_processor import process_image
from risk_evaluator import evaluate_risk


# Lấy đường dẫn thư mục hiện tại: .../Smart-Home-Security-System/ai
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# Ảnh nhận từ MQTT sẽ được lưu vào ai/captured_images
CAPTURE_DIR = os.path.join(BASE_DIR, "captured_images")


# Nếu MQTT Broker chạy trên chính laptop này thì để localhost
MQTT_BROKER = "localhost"
MQTT_PORT = 1883

# Topic ESP32-CAM gửi ảnh lên
IMAGE_TOPIC = "home/camera/image"

# Topic AI gửi kết quả nhận diện cho dashboard
AI_RESULT_TOPIC = "home/ai/result"

# Topic gửi lệnh cảnh báo về ESP32-CAM
ALARM_TOPIC = "home/device/alarm"


def save_image_from_mqtt(payload):
    """
    Nhận dữ liệu ảnh dạng bytes từ MQTT và lưu thành file .jpg
    """

    os.makedirs(CAPTURE_DIR, exist_ok=True)

    file_name = datetime.now().strftime("mqtt_%Y%m%d_%H%M%S.jpg")
    image_path = os.path.join(CAPTURE_DIR, file_name)

    with open(image_path, "wb") as f:
        f.write(payload)

    return image_path


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Đã kết nối MQTT Broker")
        client.subscribe(IMAGE_TOPIC)
        print(f"Đang lắng nghe ảnh từ topic: {IMAGE_TOPIC}")
    else:
        print("Kết nối MQTT thất bại, mã lỗi:", rc)


def on_message(client, userdata, msg):
    print(f"Nhận dữ liệu từ topic: {msg.topic}")

    if msg.topic != IMAGE_TOPIC:
        return

    try:
        # 1. Lưu ảnh nhận từ ESP32-CAM
        image_path = save_image_from_mqtt(msg.payload)
        print("Đã lưu ảnh:", image_path)

        # 2. Gọi AI xử lý ảnh
        result = process_image(image_path)
        print("Kết quả AI:", result)

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

        # 3. Gửi kết quả AI lên MQTT cho dashboard
        client.publish(
            AI_RESULT_TOPIC,
            json.dumps(result, ensure_ascii=False),
            retain=False
        )

        # 4. Nếu phát hiện người lạ thì gửi lệnh bật còi/LED
        if result["danger_level"] >= 3:
            alarm_payload = {
                "alarm": result["alarm"],
                "danger_level": result["danger_level"],
                "risk_label": result["risk_label"],
                "message": result["risk_message"]
            }

            client.publish(
                ALARM_TOPIC,
                json.dumps(alarm_payload, ensure_ascii=False),
                retain=False
            )

            print("Đã gửi lệnh cảnh báo:", alarm_payload)

        else:
            alarm_payload = {
                "alarm": "off",
                "danger_level": result["danger_level"],
                "risk_label": result["risk_label"],
                "message": result["risk_message"]
            }

            client.publish(
                ALARM_TOPIC,
                json.dumps(alarm_payload, ensure_ascii=False),
                retain=False
            )

            print("Không cần bật cảnh báo:", alarm_payload)

            

    except Exception as e:
        error_result = {
            "status": "error",
            "message": str(e),
            "alert": False,
            "people": []
        }

        client.publish(
            AI_RESULT_TOPIC,
            json.dumps(error_result, ensure_ascii=False),
            retain=False
        )

        print("Lỗi xử lý ảnh:", e)


def main():
    client = mqtt.Client()

    client.on_connect = on_connect
    client.on_message = on_message

    print("Đang kết nối MQTT Broker...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)

    client.loop_forever()


if __name__ == "__main__":
    main()