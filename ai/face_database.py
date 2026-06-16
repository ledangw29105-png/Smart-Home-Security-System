import os
import pickle
import face_recognition


KNOWN_FACES_DIR = "known_faces"
DATABASE_FILE = "face_database.pkl"


def build_database():
    known_encodings = []
    known_names = []

    if not os.path.exists(KNOWN_FACES_DIR):
        print(f"Chưa có thư mục {KNOWN_FACES_DIR}")
        return

    for person_name in os.listdir(KNOWN_FACES_DIR):
        person_path = os.path.join(KNOWN_FACES_DIR, person_name)

        if not os.path.isdir(person_path):
            continue

        for image_name in os.listdir(person_path):
            image_path = os.path.join(person_path, image_name)

            if not image_name.lower().endswith((".jpg", ".jpeg", ".png")):
                continue

            try:
                image = face_recognition.load_image_file(image_path)
                encodings = face_recognition.face_encodings(image)

                if len(encodings) > 0:
                    known_encodings.append(encodings[0])
                    known_names.append(person_name)
                    print(f"Added {person_name} from {image_name}")
                else:
                    print(f"No face found in {image_path}, skipping.")

            except Exception as e:
                print(f"Error processing {image_path}: {e}")

    data = {
        "encodings": known_encodings,
        "names": known_names
    }

    with open(DATABASE_FILE, "wb") as f:
        pickle.dump(data, f)

    print("Tạo database hoàn tất.")
    print(f"Số khuôn mặt đã lưu: {len(known_encodings)}")
    print(f"File database: {DATABASE_FILE}")


def load_database():
    if not os.path.exists(DATABASE_FILE):
        raise FileNotFoundError(
            "Chưa có face_database.pkl. Hãy chạy face_database.py trước."
        )

    with open(DATABASE_FILE, "rb") as f:
        data = pickle.load(f)

    return data["encodings"], data["names"]


if __name__ == "__main__":
    build_database()