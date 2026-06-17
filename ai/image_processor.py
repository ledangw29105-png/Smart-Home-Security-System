import os
import cv2
import face_recognition
from datetime import datetime
from ai_recognition import recognize_faces

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

CAPTURE_DIR = os.path.join(BASE_DIR, "captured_images")
PROCESSED_DIR = os.path.join(BASE_DIR, "processed_images")


def process_image(image_path, tolerance=0.5):
    # Đọc ảnh bằng OpenCV
    image_bgr = cv2.imread(image_path)

    # Nếu không đọc được ảnh
    if image_bgr is None:
        return {
            "status": "error",
            "message": f"Không thể đọc ảnh từ {image_path}",
            "alert": False,
            "people": []
        }

    # Giảm kích thước ảnh để xử lý nhanh hơn
    small_image_bgr = cv2.resize(image_bgr, (0, 0), fx=0.25, fy=0.25)

    # OpenCV đọc ảnh theo BGR, còn face_recognition dùng RGB
    small_image_rgb = cv2.cvtColor(small_image_bgr, cv2.COLOR_BGR2RGB)

    # Phát hiện vị trí khuôn mặt
    face_locations = face_recognition.face_locations(small_image_rgb)

    if len(face_locations) == 0:
        return {
            "status": "no_faces",
            "message": "Không phát hiện khuôn mặt nào trong ảnh.",
            "alert": False,
            "people": []
        }

    # Nhận diện khuôn mặt
    results = recognize_faces(small_image_rgb, face_locations, tolerance)

    people = []
    has_unknown = False

    for result in results:
        name = result["name"]
        status = result["status"]
        distance = result["distance"]

        if status == "unknown":
            has_unknown = True

        people.append({
            "name": name,
            "status": status,
            "distance": distance
        })

        # Lấy vị trí khuôn mặt
        top, right, bottom, left = result["location"]

        # Vì ảnh đã bị resize 0.25 nên cần nhân lại 4 lần
        top *= 4
        right *= 4
        bottom *= 4
        left *= 4

        # Chọn màu khung
        if status == "known":
            color = (0, 255, 0)      # xanh lá: người quen
            label = name
        else:
            color = (0, 0, 255)      # đỏ: người lạ
            label = "Unknown"

        # Vẽ khung khuôn mặt
        cv2.rectangle(image_bgr, (left, top), (right, bottom), color, 2)

        # Vẽ nền chữ
        cv2.rectangle(
            image_bgr,
            (left, bottom - 35),
            (right, bottom),
            color,
            cv2.FILLED
        )

        # Ghi tên người lên ảnh
        cv2.putText(
            image_bgr,
            label,
            (left + 6, bottom - 6),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (255, 255, 255),
            1
        )

    # Tạo thư mục lưu ảnh đã xử lý nếu chưa có
    os.makedirs(PROCESSED_DIR, exist_ok=True)

    # Tạo tên file ảnh đã xử lý
    output_name = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + os.path.basename(image_path)
    output_path = os.path.join(PROCESSED_DIR, output_name)

    # Lưu ảnh đã vẽ khung
    cv2.imwrite(output_path, image_bgr)

    return {
        "status": "success",
        "message": "Xử lý ảnh thành công",
        "alert": has_unknown,
        "people": people,
        "output_path": output_path
    }
if __name__ == "__main__":
    test_image = os.path.join(CAPTURE_DIR, "test.jpg")
    result = process_image(test_image)
    print(result)