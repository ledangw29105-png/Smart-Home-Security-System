import os
import pickle
import numpy as np
import face_recognition

DATABASE_FILE = "face_database.pkl"


def load_database():
    """
    Đọc database khuôn mặt đã tạo từ file face_database.pkl
    """

    if not os.path.exists(DATABASE_FILE):
        raise FileNotFoundError(
            "Chưa tìm thấy face_database.pkl. Hãy chạy file face_database.py trước."
        )

    with open(DATABASE_FILE, "rb") as f:
        data = pickle.load(f)

    known_encodings = data["encodings"]
    known_names = data["names"]

    return known_encodings, known_names


def recognize_faces(image_rgb, face_locations, tolerance=0.5):
    """
    Nhận diện các khuôn mặt trong ảnh.

    image_rgb: ảnh dạng RGB
    face_locations: vị trí các khuôn mặt đã phát hiện
    tolerance: ngưỡng nhận diện, càng nhỏ càng nghiêm ngặt
    """

    known_encodings, known_names = load_database()

    # Trích xuất vector đặc trưng của các khuôn mặt trong ảnh mới
    face_encodings = face_recognition.face_encodings(
        image_rgb,
        face_locations
    )

    results = []

    for face_encoding, face_location in zip(face_encodings, face_locations):

        # Nếu database rỗng thì coi tất cả là người lạ
        if len(known_encodings) == 0:
            results.append({
                "name": "Unknown",
                "status": "unknown",
                "distance": None,
                "location": face_location
            })
            continue

        # Tính khoảng cách giữa mặt mới và các mặt trong database
        distances = face_recognition.face_distance(
            known_encodings,
            face_encoding
        )

        # Lấy người có khoảng cách nhỏ nhất
        best_match_index = np.argmin(distances)
        best_distance = distances[best_match_index]

        # Nếu khoảng cách nhỏ hơn tolerance thì nhận là người quen
        if best_distance < tolerance:
            name = known_names[best_match_index]
            status = "known"
        else:
            name = "Unknown"
            status = "unknown"

        results.append({
            "name": name,
            "status": status,
            "distance": float(best_distance),
            "location": face_location
        })

    return results