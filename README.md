# 🦾 Endüstriyel Robot Kol 

ESP32 tabanlı 5 eksenli pick & place robot kolu. Kamera modülü renk algılar, kol
küpleri kırmızı/mavi kutulara sıralar.

---

## 📁 Klasör Yapısı

```
robot_kol_
│
├── esp32_s3_grid/         → Ana kontrol kodu (ESP32-S3)
│   └── esp32_s3_grid.ino      Kameradan gelen görevi alır, PCA9685 ile servoları sürer
│
├── robot_kol_sim/         → Pick & Place simülasyonu (kamerasız test)
│   └── robot_kol_sim.ino      5 eksenli, A→B arası otomatik döngü
│
└── araçlar/               → PC tarafı araçlar
    ├── camera_server.py       PC'den ESP32-CAM görüntüsü alır, OpenCV ile renk tespiti yapar, S3'e HTTP komut gönderir
```

---

## 🔌 Donanım

| Bileşen | Model | Bağlantı |
|---|---|---|
| Ana kontrol | ESP32-S3 | I2C: SDA=17, SCL=18 |
| Kamera | ESP32-CAM (AI Thinker) | UART → S3: RX=16, TX=15 |
| Servo sürücü | PCA9685 | I2C adres: 0x40 |
| Taban/Omuz/Dirsek | 20kg servo | CH 0, 1, 2 |
| Bilek | SG90 | CH 3 | 
| Gripper | MG90S | CH 4 |

---

## 🚀 Kullanım

### Mod 1 — PC'den görüntü işleme (camera_server.py)
PC kameradan görüntü çeker, OpenCV ile renk tespiti yapar ve S3'e HTTP üzerinden komut gönderir.

1. `araçlar/camera_server.py` dosyasını aç
2. `S3_IP` ve `CAMERA_IP` değerlerini kendi IP adreslerinle güncelle
3. Gereksinimleri kur: `pip install opencv-python numpy requests`
4. Çalıştır: `python camera_server.py`

### Mod 2 — ESP32-CAM ile gömülü grid tarama
Kamera doğrudan ESP32 üzerinde 3x3 grid tarar, UART ile S3'e gönderir.

1. **i2c_scan** yükle → PCA9685'in `0x40`'ta göründüğünü doğrula  
2. **esp32_s3_grid** → ESP32-S3'e yükle  
3. **esp32_cam_grid** → ESP32-CAM'e yükle  
4. Sistem açılınca kamera otomatik tarar, kol sıralamayı başlatır

### Pozisyon Kalibrasyonu
`esp32_s3_grid.ino` içindeki `gridPozisyonlari[]` dizisini sahada deneyerek ayarla.  
`KIRMIZI_KUTU` ve `MAVI_KUTU` struct'larını da kutunun gerçek yerine göre güncelle.

---

## 📡 Haberleşme Protokolü (CAM ↔ S3)

| Yön | Mesaj | Anlam |
|---|---|---|
| CAM → S3 | `0,RED` | Pozisyon 0'da kırmızı küp var |
| CAM → S3 | `BITTI` | Tüm grid tarandı, harekete geç |
| S3 → CAM | `HAZIR` | Kol bitti, yeni tarama yapabilirsin |
