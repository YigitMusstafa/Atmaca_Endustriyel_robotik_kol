/*
 * ============================================================
 *  5 EKSENLİ ROBOT KOL - PICK & PLACE SİMÜLASYON
 *  Donanım : ESP32 + PCA9685 (0x40)
 *  Eksen 0  : Baz rotasyon       — 20 kg servo (CH 0)
 *  Eksen 1  : Alt kol (omuz)     — 20 kg servo (CH 1)
 *  Eksen 2  : Üst kol (dirsek)   — 20 kg servo (CH 2)
 *  Eksen 3  : Bilek pitch         — MG90S       (CH 3)
 *  Eksen 4  : Gripper (tutucu)   — MG90S       (CH 4)
 *
 *  Kamera henüz bağlı değil → A noktasından sarı kutu alıp
 *  B noktasına koymayı simüle eder.
 *
 *  Özellikler:
 *   - Sıralı hareket  : bir eksen tamamen hareket edince sıradaki başlar
 *   - Pozisyon tutma  : hareket bitince PWM sinyali DEVAM EDER (sürünme yok)
 *   - Yavaş geçiş     : moveServoSlow() fonksiyonu adım adım ilerler
 *   - Servo başına min/max sınırları tanımlı (mekanik koruma)
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ---------- I2C ----------
#define SDA_PIN  17
#define SCL_PIN  18

// ---------- PCA9685 ----------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define PWM_FREQ 50   // Hz

// ---------- Eksen tanımları ----------
#define CH_BASE     0
#define CH_SHOULDER 1
#define CH_ELBOW    2
#define CH_WRIST    3
#define CH_GRIPPER  4

// ---------- Pulse sınırları (her servo için ayrı) ----------
// 20 kg'lık servolarda daha geniş aralık güvenli
struct ServoConfig {
  int channel;
  int pulseMin;   // 0°  karşılığı
  int pulseMax;   // 180° karşılığı
  int safeMin;    // mekanik min açı (derece)
  int safeMax;    // mekanik max açı (derece)
};

ServoConfig servos[5] = {
  //  CH    pulMin  pulMax  safMin  safMax
  {  0,    150,    500,      0,    180  },   // Baz
  {  1,    150,    500,     30,    150  },   // Omuz   (mekanik sınır)
  {  2,    150,    500,     20,    160  },   // Dirsek
  {  3,    180,    480,      0,    180  },   // Bilek  (MG90S)
  {  4,    200,    450,      0,    120  },   // Gripper (MG90S, 0=kapalı 120=açık)
};

// ---------- Yavaş hareket parametresi ----------
#define STEP_DELAY_MS  15    // Her adım arası bekleme (ms) → küçük=hızlı, büyük=yavaş
#define STEP_DEG        1    // Kaç derece atlayarak ilerlesin

// ---------- Mevcut açılar (hafıza - güç kesilince servo konumda kalır) ----------
int currentAngle[5] = { 90, 90, 90, 90, 0 };   // Başlangıç home pozisyonu

// ============================================================
//  Yardımcı: açı → pulse dönüşümü (servo başına kalibrasyon)
// ============================================================
int angleToPulse(int servoIdx, int angle) {
  ServoConfig& s = servos[servoIdx];
  // Açıyı güvenli sınırlar içine kısıt
  angle = constrain(angle, s.safeMin, s.safeMax);
  return map(angle, 0, 180, s.pulseMin, s.pulseMax);
}

// ============================================================
//  Anlık konuma yaz (PWM devam eder → servo sabit durur)
// ============================================================
void setServo(int servoIdx, int angle) {
  ServoConfig& s = servos[servoIdx];
  angle = constrain(angle, s.safeMin, s.safeMax);
  pwm.setPWM(s.channel, 0, angleToPulse(servoIdx, angle));
  currentAngle[servoIdx] = angle;
}

// ============================================================
//  Yavaş geçiş: mevcut açıdan hedef açıya adım adım git
// ============================================================
void moveServoSlow(int servoIdx, int targetAngle, int stepDelay = STEP_DELAY_MS) {
  ServoConfig& s = servos[servoIdx];
  targetAngle = constrain(targetAngle, s.safeMin, s.safeMax);

  int from = currentAngle[servoIdx];
  int direction = (targetAngle > from) ? STEP_DEG : -STEP_DEG;

  Serial.printf("  [CH%d] %d° → %d°\n", s.channel, from, targetAngle);

  for (int ang = from; ang != targetAngle; ang += direction) {
    // Son adımda tam hedefe otur
    if ((direction > 0 && ang > targetAngle) ||
        (direction < 0 && ang < targetAngle)) {
      ang = targetAngle;
    }
    pwm.setPWM(s.channel, 0, angleToPulse(servoIdx, ang));
    currentAngle[servoIdx] = ang;
    delay(stepDelay);
  }
  // Kesin olarak hedefte sabitle
  pwm.setPWM(s.channel, 0, angleToPulse(servoIdx, targetAngle));
  currentAngle[servoIdx] = targetAngle;
  delay(50); // küçük stabilizasyon gecikmesi
}

// ============================================================
//  Tüm eksenleri home pozisyonuna götür (sıralı, yavaş)
// ============================================================
void goHome() {
  Serial.println("\n=== HOME POZİSYONA DÖN ===");
  moveServoSlow(CH_WRIST,    90);
  moveServoSlow(CH_ELBOW,    90);
  moveServoSlow(CH_SHOULDER, 90);
  moveServoSlow(CH_BASE,     90);
  // Gripper açık kalsın (home'da bırakma)
  moveServoSlow(CH_GRIPPER, 100);
  Serial.println("  Home tamam.\n");
  delay(500);
}

// ============================================================
//  PICK sekansı — A noktasından sarı kutuyu al
//  A noktası varsayım: Baz=45°, Omuz=110°, Dirsek=60°, Bilek=80°
// ============================================================
void pickFromA() {
  Serial.println("\n=== PICK: A NOKTASINA GİT ===");

  // 1. Gripperı önce aç
  Serial.println("  [1/6] Gripper açılıyor...");
  moveServoSlow(CH_GRIPPER, 100);
  delay(300);

  // 2. Baz döndür
  Serial.println("  [2/6] Baz konumlanıyor...");
  moveServoSlow(CH_BASE, 45);

  // 3. Omuzu indir
  Serial.println("  [3/6] Omuz iniyor...");
  moveServoSlow(CH_SHOULDER, 110);

  // 4. Dirseği ayarla
  Serial.println("  [4/6] Dirsek ayarlanıyor...");
  moveServoSlow(CH_ELBOW, 60);

  // 5. Bileği hizala
  Serial.println("  [5/6] Bilek hizalanıyor...");
  moveServoSlow(CH_WRIST, 80);

  delay(400); // Kolun stabilize olması için

  // 6. Gripper kapat — kutuyu yakala
  Serial.println("  [6/6] Gripper kapanıyor (sarı kutu yakalandı)...");
  moveServoSlow(CH_GRIPPER, 10, 20);  // Daha yavaş kapat
  delay(600);

  Serial.println("  PICK tamamlandi!\n");
}

// ============================================================
//  Kaldırma hareketi — yerden kaldır (ara pozisyon)
// ============================================================
void liftUp() {
  Serial.println("=== KALDIR ===");
  moveServoSlow(CH_ELBOW,    90);
  moveServoSlow(CH_SHOULDER, 80);
  Serial.println("  Kaldırma tamamlandı.\n");
  delay(300);
}

// ============================================================
//  PLACE sekansı — B noktasına sarı kutuyu bırak
//  B noktası varsayım: Baz=135°, Omuz=105°, Dirsek=65°, Bilek=85°
// ============================================================
void placeAtB() {
  Serial.println("=== PLACE: B NOKTASINA GİT ===");

  // 1. Baz B tarafına döndür
  Serial.println("  [1/5] Baz B tarafına dönüyor...");
  moveServoSlow(CH_BASE, 135);

  // 2. Omuzu B pozisyonuna getir
  Serial.println("  [2/5] Omuz B pozisyonuna iniyor...");
  moveServoSlow(CH_SHOULDER, 105);

  // 3. Dirseği B pozisyonuna getir
  Serial.println("  [3/5] Dirsek B pozisyonuna iniyor...");
  moveServoSlow(CH_ELBOW, 65);

  // 4. Bileği hizala
  Serial.println("  [4/5] Bilek hizalanıyor...");
  moveServoSlow(CH_WRIST, 85);

  delay(400);

  // 5. Gripper aç — kutuyu bırak
  Serial.println("  [5/5] Gripper açılıyor (sarı kutu bırakıldı)...");
  moveServoSlow(CH_GRIPPER, 100, 20);
  delay(600);

  Serial.println("  PLACE tamamlandi!\n");
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n===================================");
  Serial.println(" 5 EKSENLİ ROBOT KOL - BAŞLATILIYOR");
  Serial.println("===================================\n");

  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(PWM_FREQ);
  delay(100);

  // Tüm servoları başlangıç açısına gönder (yavaş)
  Serial.println("Başlangıç home pozisyonu...");
  for (int i = 0; i < 5; i++) {
    setServo(i, currentAngle[i]);
    delay(300);
  }
  delay(1000);

  Serial.println("Hazır. Pick & Place döngüsü başlıyor...\n");
}

// ============================================================
//  ANA DÖNGÜ — sürekli A'dan al, B'ye koy
// ============================================================
void loop() {
  Serial.println("========== YENİ DÖNGÜ ==========");

  pickFromA();    // A noktasından al
  liftUp();       // Kaldır
  placeAtB();     // B noktasına koy
  goHome();       // Home'a dön

  Serial.println("Döngü bitti. 2 saniye bekleniyor...\n");
  delay(2000);
}
