from datetime import datetime, timedelta

unknown_history = []  # Lưu thời điểm phát hiện người lạ


def is_night_time():
    """
    Kiểm tra hiện tại có phải ban đêm không.
    Quy ước: từ 22:00 đến trước 06:00 là ban đêm.
    """
    current_hour = datetime.now().hour
    return current_hour >= 22 or current_hour < 6


def count_recent_unknowns(window_seconds=60):
    """
    Đếm số lần phát hiện người lạ trong khoảng thời gian gần đây.
    Mặc định: 60 giây.
    """
    now = datetime.now()
    window_start = now - timedelta(seconds=window_seconds)

    recent_unknowns = [
        t for t in unknown_history
        if window_start <= t <= now
    ]

    return len(recent_unknowns)


def evaluate_risk(ai_result, motion_detected=True, motion_duration=0):
    """
    Đánh giá mức độ nguy hiểm dựa trên:
    - Có chuyển động hay không
    - Có khuôn mặt hay không
    - Người quen hay người lạ
    - Người lạ xuất hiện nhiều lần
    - Ban đêm
    - Thời gian chuyển động
    """

    # Level 0: Không có chuyển động
    if not motion_detected:
        return {
            "danger_level": 0,
            "risk_label": "Không có chuyển động",
            "message": "Không có chuyển động, không cần cảnh báo",
            "alarm": "off"
        }

    # Lấy trạng thái AI
    status = ai_result.get("status")

    # Level 1: Có chuyển động nhưng không phát hiện khuôn mặt
    if status == "no_faces":
        return {
            "danger_level": 1,
            "risk_label": "Có chuyển động nhưng không thấy mặt",
            "message": "Có chuyển động nhưng không phát hiện khuôn mặt",
            "alarm": "off"
        }

    # Level 1: Lỗi xử lý ảnh
    if status == "error":
        return {
            "danger_level": 1,
            "risk_label": "Lỗi xử lý ảnh",
            "message": ai_result.get("message", "Có lỗi trong quá trình xử lý ảnh"),
            "alarm": "off"
        }

    people = ai_result.get("people", [])

    has_unknown = any(person.get("status") == "unknown" for person in people)
    has_known = any(person.get("status") == "known" for person in people)

    # Level 2: Chỉ thấy người quen, không có người lạ
    if has_known and not has_unknown:
        return {
            "danger_level": 2,
            "risk_label": "Người quen",
            "message": "Phát hiện người quen, không cần cảnh báo",
            "alarm": "off"
        }

    # Nếu có người lạ thì lưu lại thời điểm
    if has_unknown:
        unknown_history.append(datetime.now())

    recent_unknown_count = count_recent_unknowns(window_seconds=60)
    night_time = is_night_time()

    # Level 5: Người lạ + ban đêm + đứng lâu
    if has_unknown and night_time and motion_duration >= 30:
        return {
            "danger_level": 5,
            "risk_label": "Nguy hiểm cao",
            "message": "Người lạ xuất hiện vào ban đêm và đứng lâu trước camera",
            "alarm": "strong_on"
        }

    # Level 5: Người lạ + ban đêm + xuất hiện nhiều lần
    if has_unknown and night_time and recent_unknown_count >= 2:
        return {
            "danger_level": 5,
            "risk_label": "Nguy hiểm cao",
            "message": "Người lạ xuất hiện nhiều lần vào ban đêm",
            "alarm": "strong_on"
        }

    # Level 4: Người lạ xuất hiện nhiều lần trong 60 giây
    if has_unknown and recent_unknown_count >= 2:
        return {
            "danger_level": 4,
            "risk_label": "Người lạ lặp lại",
            "message": "Người lạ xuất hiện nhiều lần trong thời gian ngắn",
            "alarm": "on"
        }

    # Level 3: Người lạ lần đầu
    if has_unknown:
        return {
            "danger_level": 3,
            "risk_label": "Người lạ",
            "message": "Phát hiện người lạ lần đầu",
            "alarm": "on"
        }

    # Trường hợp không rõ
    return {
        "danger_level": 1,
        "risk_label": "Không xác định",
        "message": "Không xác định rõ trạng thái",
        "alarm": "off"
    }