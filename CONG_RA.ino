
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// =====================================================
// VIN PARKING - ESP32-S3 CONG RA
// GPIO giu nguyen theo mach hien tai
// =====================================================

// RFID - MFRC522
#define RFID_SS   10
#define RFID_RST  9
#define RFID_SCK  12
#define RFID_MISO 13
#define RFID_MOSI 11

MFRC522 rfid(RFID_SS, RFID_RST);

// LCD I2C
#define LCD_SDA 4
#define LCD_SCL 5

LiquidCrystal_I2C lcd(0x27, 16, 2);

// SERVO + BUZZER
#define SERVO_PIN  17
#define BUZZER_PIN 18

Servo barrier;
String currentExitToken = "";
uint32_t requestCounter = 0;

// 4 cam bien IR
#define IR1 6
#define IR2 7
#define IR3 8
#define IR4 14

// =====================================================
// BIEN TRANG THAI
// LOW = co xe, HIGH = trong
// =====================================================

int p1 = HIGH;
int p2 = HIGH;
int p3 = HIGH;
int p4 = HIGH;

int occupied = 0;
int freeSlots = 4;

// =====================================================
// DOC UID RFID
// =====================================================

String getUIDString() {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      uid += ":";
    }
  }

  uid.toUpperCase();
  return uid;
}

// =====================================================
// DOC TRANG THAI 4 VI TRI
// =====================================================

void readParking() {
  p1 = digitalRead(IR1);
  p2 = digitalRead(IR2);
  p3 = digitalRead(IR3);
  p4 = digitalRead(IR4);

  occupied = 0;

  if (p1 == LOW) occupied++;
  if (p2 == LOW) occupied++;
  if (p3 == LOW) occupied++;
  if (p4 == LOW) occupied++;

  freeSlots = 4 - occupied;
}

// =====================================================
// HIEN THI LCD
// =====================================================

void showParking() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("P1:");
  lcd.print(p1 == LOW ? "X" : "-");
  lcd.print(" P2:");
  lcd.print(p2 == LOW ? "X" : "-");
  lcd.print(" P3:");
  lcd.print(p3 == LOW ? "X" : "-");

  lcd.setCursor(0, 1);
  lcd.print("P4:");
  lcd.print(p4 == LOW ? "X" : "-");
  lcd.print(" D:");
  lcd.print(occupied);
  lcd.print(" T:");
  lcd.print(freeSlots);
}

// =====================================================
// GUI TRANG THAI CAM BIEN DEN RASPBERRY PI
// Dinh dang: PARKING,0,1,0,1
// =====================================================

void printParkingSerial() {
  Serial.print("PARKING,");
  Serial.print(p1);
  Serial.print(",");
  Serial.print(p2);
  Serial.print(",");
  Serial.print(p3);
  Serial.print(",");
  Serial.println(p4);

  Serial.print("[IR] P1:");
  Serial.print(p1 == LOW ? "CO XE" : "TRONG");

  Serial.print(" P2:");
  Serial.print(p2 == LOW ? "CO XE" : "TRONG");

  Serial.print(" P3:");
  Serial.print(p3 == LOW ? "CO XE" : "TRONG");

  Serial.print(" P4:");
  Serial.print(p4 == LOW ? "CO XE" : "TRONG");

  Serial.print(" | Do:");
  Serial.print(occupied);

  Serial.print(" | Trong:");
  Serial.println(freeSlots);
}

// =====================================================
// BUZZER
// =====================================================

void beep(int times, int duration = 120) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);

    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// =====================================================
// YEU CAU PI XAC THUC QUYEN RA
//
// ESP32 -> Pi: EXIT_REQUEST,UID
// Pi -> ESP32: RESULT,EXIT_ALLOWED
//             RESULT,EXIT_DENIED
//
// Khong co phan hoi / phan hoi khong hop le:
// TU CHOI, KHONG MO BARRIER
// =====================================================

bool requestExitAuthorization(String uid) {
  // Mỗi yêu cầu có token riêng để không nhận nhầm phản hồi cũ.
  requestCounter++;
  currentExitToken = String(millis()) + "-" + String(requestCounter);

  String request = "EXIT_REQUEST," + uid + "," + currentExitToken;
  Serial.println(request);

  String allowedResponse =
      "RESULT,EXIT_ALLOWED," + uid + "," + currentExitToken;
  String deniedResponse =
      "RESULT,EXIT_DENIED," + uid + "," + currentExitToken;

  unsigned long start = millis();

  while (millis() - start < 40000UL) {
    if (Serial.available() > 0) {
      String response = Serial.readStringUntil('\n');
      response.trim();

      // Chỉ nhận phản hồi đúng UID và đúng token hiện tại.
      if (response == allowedResponse) {
        return true;
      }

      if (response == deniedResponse) {
        return false;
      }
    }

    delay(10);
  }

  Serial.println("EXIT_AUTH_TIMEOUT");
  return false;
}

// =====================================================
// MO BARRIER
// Chi duoc goi khi Pi tra EXIT_ALLOWED
// =====================================================

void openBarrier() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("THE HOP LE");

  lcd.setCursor(0, 1);
  lcd.print("MO BARRIER");

  beep(1);

  // Mo barrier
  for (int angle = 0; angle <= 90; angle += 3) {
    barrier.write(angle);
    delay(20);
  }

  delay(3000);

  // Dong barrier
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DANG DONG");

  for (int angle = 90; angle >= 0; angle -= 3) {
    barrier.write(angle);
    delay(20);
  }

  delay(500);

  // Cap nhat lai vi tri do
  readParking();
  showParking();
}

// =====================================================
// TU CHOI THE
// =====================================================

void rejectCard() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("THE KHONG HOP");

  lcd.setCursor(0, 1);
  lcd.print("TU CHOI!");

  beep(3, 100);
  delay(1500);

  readParking();
  showParking();
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // RFID
  SPI.begin(
    RFID_SCK,
    RFID_MISO,
    RFID_MOSI,
    RFID_SS
  );

  rfid.PCD_Init();

  // LCD
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();

  // Servo - mac dinh dong
  barrier.setPeriodHertz(50);
  barrier.attach(SERVO_PIN, 500, 2400);
  barrier.write(0);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // IR
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);
  pinMode(IR4, INPUT);

  // Khoi dong
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VIN PARKING");

  lcd.setCursor(0, 1);
  lcd.print("CONG RA");

  delay(1500);

  readParking();
  showParking();
  printParkingSerial();

  Serial.println("VIN PARKING - CONG RA READY");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Cap nhat cam bien
  readParking();

  // Doc the RFID
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      String uid = getUIDString();

      Serial.println("UID:" + uid);

      // Tuyet doi khong kiem tra UID bang danh sach viet cung
      // Pi la ben quyet dinh cho phep hoac tu choi
      bool allowed = requestExitAuthorization(uid);

      if (allowed) {
        openBarrier();

        // Gui su kien sau khi ESP32 chay xong chu trinh dong
        Serial.println("EXIT_COMPLETED," + uid + "," + currentExitToken);
      } else {
        rejectCard();
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      delay(500);
    }
  }

  // Gui trang thai IR moi giay
  static unsigned long lastDisplay = 0;

  if (millis() - lastDisplay >= 1000UL) {
    lastDisplay = millis();

    readParking();
    showParking();
    printParkingSerial();
  }

  delay(10);
}
