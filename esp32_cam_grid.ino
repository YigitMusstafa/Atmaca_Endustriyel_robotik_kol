#include "esp_camera.h"

// =============================================
// AI Thinker ESP32-CAM Pin Tanımlamaları
// =============================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =============================================
// Grid ve Görüntü Ayarları
// QQVGA = 160x120 piksel, 3x3 grid
// Gri kare (boş/home) = pozisyon 3 (orta sol)
// =============================================
#define FRAME_W       160
#define FRAME_H       120
#define GRID_COLS     3
#define GRID_ROWS     3
#define GRID_TOPLAM   9
#define BOS_POZISYON  3     // Gri kare (robot home), bu pozisyon atlanır

// Renk algılama eşiği: bir hücrede kaç piksel gerekli
#define ESIK_PIKSEL   30

// Hücrenin ortasından alınan örnek bölge (piksel)
// Tüm hücreyi değil, merkezini tarar → daha güvenilir
#define MERKEZ_ORAN   3   // Hücrenin 1/3'ü merkez bölge

// =============================================
// Gönderim gecikmesi (ms) - S3 işlerken bekle
// =============================================
#define GONDERIM_GECIKMESI 500

void setup() {
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QQVGA;  // 160x120
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera baslatılamadi! Hata: 0x%x\n", err);
    return;
  }
  Serial.println("ESP32-CAM hazir! Grid tarama basliyor...");
  delay(1000);
}

// =============================================
// Belirli bir grid hücresindeki rengi döndürür
// pos: 0-8 (sol üst → sağ alt, satır satır)
// Geri dönüş: "RED", "BLUE", "NONE"
// =============================================
String hucreRenkAlgila(camera_fb_t *fb, int pos) {
  int satir = pos / GRID_COLS;
  int sutun = pos % GRID_COLS;

  // Hücrenin piksel sınırları
  int hucre_w = FRAME_W / GRID_COLS;  // ~53px
  int hucre_h = FRAME_H / GRID_ROWS;  // ~40px

  int hucre_x = sutun * hucre_w;
  int hucre_y = satir * hucre_h;

  // Hücre merkezini al (kenarları değil)
  int merkez_pad_x = hucre_w / MERKEZ_ORAN;
  int merkez_pad_y = hucre_h / MERKEZ_ORAN;

  int x_baslangic = hucre_x + merkez_pad_x;
  int x_bitis     = hucre_x + hucre_w - merkez_pad_x;
  int y_baslangic = hucre_y + merkez_pad_y;
  int y_bitis     = hucre_y + hucre_h - merkez_pad_y;

  int redCount  = 0;
  int blueCount = 0;

  for (int y = y_baslangic; y < y_bitis; y++) {
    for (int x = x_baslangic; x < x_bitis; x++) {
      int idx = (y * FRAME_W + x) * 2;
      if (idx + 1 >= (int)fb->len) continue;

      uint16_t pixel = fb->buf[idx] | (fb->buf[idx + 1] << 8);

      int r = ((pixel >> 11) & 0x1F) * 255 / 31;
      int g = ((pixel >> 5)  & 0x3F) * 255 / 63;
      int b = ( pixel        & 0x1F) * 255 / 31;

      if (r > 120 && g < 90 && b < 90)  redCount++;
      if (b > 120 && r < 90 && g < 120) blueCount++;
    }
  }

  if (redCount > blueCount && redCount > ESIK_PIKSEL)  return "RED";
  if (blueCount > redCount && blueCount > ESIK_PIKSEL) return "BLUE";
  return "NONE";
}

// =============================================
// Tüm gridi tara, renk bulunanları S3'e gönder
// Format: "POZ,RENK\n" örn: "0,RED\n"
// "BITTI\n" gönderince S3 harekete geçer
// =============================================
void gridTaraVeGonder() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame alinamadi!");
    return;
  }

  Serial.println("--- Grid Tarama Basladi ---");

  for (int pos = 0; pos < GRID_TOPLAM; pos++) {
    // Gri kareyi (home pozisyonu) atla
    if (pos == BOS_POZISYON) {
      Serial.printf("Pozisyon %d: BOS (atlandi)\n", pos);
      continue;
    }

    String renk = hucreRenkAlgila(fb, pos);
    Serial.printf("Pozisyon %d: %s\n", pos, renk.c_str());

    if (renk != "NONE") {
      // S3'e gönder: "pozisyon,renk"
      String mesaj = String(pos) + "," + renk;
      Serial.println("Gonderiliyor: " + mesaj);
      Serial.println(mesaj);  // UART TX → S3'e gider
      delay(GONDERIM_GECIKMESI);
    }
  }

  // Tarama bitti sinyali
  Serial.println("BITTI");

  esp_camera_fb_return(fb);
  Serial.println("--- Grid Tarama Bitti ---");
}

void loop() {
  // S3'den "HAZIR" gelince taramayı başlat
  // Bu sayede S3 işini bitirmeden yeni tarama başlamaz
  if (Serial.available()) {
    String komut = Serial.readStringUntil('\n');
    komut.trim();
    if (komut == "HAZIR") {
      gridTaraVeGonder();
    }
  }

  // İlk çalışmada otomatik başlat
  static bool ilkCalisma = true;
  if (ilkCalisma) {
    ilkCalisma = false;
    delay(2000);  // S3'ün hazır olmasını bekle
    gridTaraVeGonder();
  }
}
