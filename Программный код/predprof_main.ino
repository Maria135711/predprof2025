#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <Keypad.h>

#define RST_PIN         5
#define SS_PIN          4
#define PIN_TRIG_1 1
#define PIN_ECHO_1 2
#define PIN_TRIG_2 47
#define PIN_ECHO_2 48
#define SERVO_PIN 7
#define SOLENOID_PIN 10
#define LSD_SDA 8
#define LSD_SCL 9
#define BUZZZER_PIN 3
#define STEP_PIN 36
#define DIR_PIN  37
#define SVET_PIN 41

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {18, 17, 16, 15};
byte colPins[COLS] = {35, 40, 38};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo servoMotor;
const byte pinLength = 4;
String savedPinCode = "";
String correctpincode = "1234";
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
LiquidCrystal_I2C lcd(0x27, 16, 2);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
long duration, cm;

void setup() {
  delay(1000);
  lcd.init();
  lcd.backlight();
  delay(1000);
  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  rfid.PCD_AntennaOff();
  rfid.PCD_AntennaOn();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  stepper.setMaxSpeed(300);
  stepper.setAcceleration(50);
  stepper.setSpeed(20);
  Serial.begin(115200);
  pinMode(PIN_TRIG_1, OUTPUT);
  pinMode(PIN_ECHO_1, INPUT);
  pinMode(PIN_TRIG_2, OUTPUT);
  pinMode(PIN_ECHO_2, INPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUZZZER_PIN, OUTPUT);
  servoMotor.attach(SERVO_PIN);
  digitalWrite(BUZZZER_PIN, 1);
  digitalWrite(SOLENOID_PIN, 1);
  close_door();
  zvuk_done();
}

void loop() {
  delay(100);
  char customKey = keypad.getKey();

  if (customKey == '#') {
    zvuk_done();
    String enteredPin = readPinCode();
    if (enteredPin != "") {
      savedPinCode = enteredPin;
      if (savedPinCode == correctpincode) {
        zvuk_done();
        open_door();
        delay(1000);
        while (dist_2() < 15) {
          delay(10);
        }
        delay(1000);
        close_door();
      } else {
        zvuk_not_done();
      }
    }
  }

  static uint32_t rebootTimer = millis();
  if (millis() - rebootTimer >= 1000) {
    rebootTimer = millis();
    digitalWrite(RST_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(RST_PIN, LOW);
    rfid.PCD_Init();
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.println("Auth error");
    return;
  }
  check_code();
}

String readPinCode() {
  String pinCode = "";
  bool isReadingPin = true;

  while (isReadingPin) {
    char customKey = keypad.getKey();

    if (customKey) {
      if (customKey == '#') {
        if (pinCode.length() == pinLength) {
          return pinCode;
        } else {
          return "";
        }
      } else if (isdigit(customKey)) {
        if (pinCode.length() < pinLength) {
          pinCode += customKey;
          Serial.println(pinCode);
        }
      }
    }
  }
  return "";
}

String read_card() {
  uint8_t dataBlock[18];
  uint8_t size = sizeof(dataBlock);
  status = rfid.MIFARE_Read(6, dataBlock, &size);
  if (status != MFRC522::STATUS_OK) {
    return "Read error";
  }
  String result;
  for (uint8_t i = 0; i < 16; i++) {
    result += String(dataBlock[i], HEX);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return result;
}

void write_card() {
  uint8_t dataToWrite4[16] = {
    0x5A, 0x6B, 0x7C, 0x8D,
    0x9E, 0xAF, 0xB0, 0xC1,
    0xD2, 0xE3, 0xF4, 0x05,
    0x16, 0x27, 0x38, 0x49
  };

  status = rfid.MIFARE_Write(6, dataToWrite4, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.println("Write error");
    return;
  } else {
    Serial.println("Write OK");
    delay(1000);
  }
}

void check_code() {
  Serial.print("cd");
  Serial.println(read_card());
  while (true) {
    if (Serial.available() > 0) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      if (response == "TRUE") {
        zvuk_done();
        print_allowed();
        open_vorota();
        delay(1000);
        while (dist_1() > 40) delay(10);
        while (dist_1() < 40) delay(10);
        close_vorota();
      } else {
        zvuk_not_done();
        print_not_allow();
        delay(1000);
        lcd.clear();
      }
      break;
    }
    delay(10);
  }
}

int dist_1() {
  const int n = 5;
  long readings[n];
  int count[n] = {0};

  for (int i = 0; i < n; i++) {
    digitalWrite(PIN_TRIG_1, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG_1, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG_1, LOW);
    duration = pulseIn(PIN_ECHO_1, HIGH);
    cm = (duration / 2) / 29.1;
    readings[i] = cm;
    delay(20);
  }

  int maxCount = 0;
  long res = readings[0];
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (readings[i] == readings[j]) {
        count[i]++;
      }
    }
    if (count[i] > maxCount) {
      maxCount = count[i];
      res = readings[i];
    }
  }
  return res;
}

int dist_2() {
  const int n = 5;
  long readings[n];
  int count[n] = {0};

  for (int i = 0; i < n; i++) {
    digitalWrite(PIN_TRIG_2, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG_2, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG_2, LOW);
    duration = pulseIn(PIN_ECHO_2, HIGH);
    cm = (duration / 2) / 29.1;
    readings[i] = cm;
    delay(20);
  }

  int maxCount = 0;
  long res = readings[0];
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (readings[i] == readings[j]) {
        count[i]++;
      }
    }
    if (count[i] > maxCount) {
      maxCount = count[i];
      res = readings[i];
    }
  }
  return res;
}

void open_door() {
  digitalWrite(SOLENOID_PIN, 0);
  delay(200);
  servoMotor.write(110);
  delay(500);
  digitalWrite(SOLENOID_PIN, 1);
}

void close_door() {
  digitalWrite(SOLENOID_PIN, 0);
  delay(200);
  servoMotor.write(20);
  delay(500);
  digitalWrite(SOLENOID_PIN, 1);
}

void print_allowed() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Allowed");
}

void print_not_allow() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Not allowed");
}

void zvuk_done() {
  digitalWrite(BUZZZER_PIN, 0);
  delay(1000);
  digitalWrite(BUZZZER_PIN, 1);
}

void zvuk_not_done() {
  digitalWrite(BUZZZER_PIN, 0);
  delay(300);
  digitalWrite(BUZZZER_PIN, 1);
  delay(300);
  digitalWrite(BUZZZER_PIN, 0);
  delay(300);
  digitalWrite(BUZZZER_PIN, 1);
}

void open_vorota() {
  stepper.moveTo(-2300);
  stepper.run();
  print_allowed();
  while (stepper.distanceToGo() != 0) {
    delay(10);
    stepper.currentPosition();
    stepper.run();
  }
  lcd.clear();
}

void close_vorota() {
  stepper.moveTo(0);
  stepper.run();
  while (stepper.distanceToGo() != 0) {
    delay(10);
    stepper.run();
  }
}