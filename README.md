# 🦾 Endüstriyel Robot Kol — v2 (Son Hal)

ESP32 tabanlı 4 eksenli pick & place robot kolu. Kamera modülü renk algılar, kol
küpleri kırmızı/mavi kutulara sıralar.

---

## 📁 Klasör Yapısı

```
robot_kol_v2/
├── esp32_cam_grid/        → Kamera kodu (ESP32-CAM, AI Thinker)
│   └── esp32_cam_grid.ino     3x3 grid tarar, renkleri S3'e UART ile gönderir
│
├── esp32_s3_grid/         → Ana kontrol kodu (ESP32-S3)
│   └── esp32_s3_grid.ino      Kameradan gelen görevi alır, PCA9685 ile servoları sürer
│
├── robot_kol_sim/         → Pick & Place simülasyonu (kamerasız test)
│   └── robot_kol_sim.ino      5 eksenli, A→B arası otomatik döngü
│
├── i2c_scan/              → Yardımcı: I2C adresi tarama
│   └── i2c_scan.ino           PCA9685 bağlantısını doğrulamak için
│
└── araçlar/               → PC tarafı araçlar
    └── servo_test.py          MicroPython ile PCA9685 servo testi
```

---

## 🔌 Donanım

| Bileşen | Model | Bağlantı |
|---|---|---|
| Ana kontrol | ESP32-S3 | I2C: SDA=17, SCL=18 |
| Kamera | ESP32-CAM (AI Thinker) | UART → S3: RX=16, TX=15 |
| Servo sürücü | PCA9685 | I2C adres: 0x40 |
| Taban/Omuz/Dirsek | 20kg servo | CH 0, 1, 2 |
| Gripper | MG90S | CH 3 |

---

## 🚀 Kullanım

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
