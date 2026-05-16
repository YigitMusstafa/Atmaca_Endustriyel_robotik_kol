import cv2
import numpy as np
import urllib.request
import requests

# --- GÜNCEL IP ADRESLERİ ---
S3_IP = "10.176.80.163" 
CAMERA_IP = "192.168.X.X" # Buraya ESP32-CAM'in IP'sini yaz abi

S3_COMMAND_URL = f"http://{S3_IP}/move"
CAMERA_URL = f"http://{CAMERA_IP}/capture"

def get_image():
    try:
        img_resp = urllib.request.urlopen(CAMERA_URL, timeout=2)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        return cv2.imdecode(imgnp, -1)
    except:
        return None

def process_image(frame):
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    # Renk filtreleri (Daha onceki gibi)
    lower_red1 = np.array([0, 150, 70]); upper_red1 = np.array([10, 255, 255])
    lower_red2 = np.array([170, 150, 70]); upper_red2 = np.array([180, 255, 255])
    mask_red = cv2.inRange(hsv, lower_red1, upper_red1) + cv2.inRange(hsv, lower_red2, upper_red2)
    
    contours_red, _ = cv2.findContours(mask_red, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    for cnt in contours_red:
        area = cv2.contourArea(cnt)
        if area > 1000:
            if area > 4000: return "KIRMIZI_BUYUK", cnt
            else: return "KIRMIZI_KUCUK", cnt
    return "BOS", None

print(f"Sistem baslatiliyor... Hedef Kol: {S3_IP}")

while True:
    frame = get_image()
    if frame is None: continue

    kutu_tipi, cnt = process_image(frame)
    
    if kutu_tipi != "BOS":
        print(f"Tespit: {kutu_tipi}. Komut gonderiliyor...")
        try:
            # S3'e komutu at ve robotun bitirmesini bekle
            r = requests.get(f"{S3_COMMAND_URL}?box={kutu_tipi}", timeout=30)
            if r.status_code == 200:
                print("Robot: Hareket tamam, yeni kutu aranıyor.")
        except:
            print("Hata: ESP32-S3'e ulasilamadi!")

    cv2.imshow("Mebrobot Vision", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

cv2.destroyAllWindows()