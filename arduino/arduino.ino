/*
    Работа команды На чиле
    Финал НТИ Умный город 2022
    Создан 09.03.2022
    Разработчики:
      Вадим
      Александр
      Никита
      Андрей
*/


// ! - POST / : - GET
#include "FastLED.h"
#include <Adafruit_PN532.h>
#include <Servo.h>
#include <TroykaOLED.h>
#include "Adafruit_TCS34725softi2c.h"

TroykaOLED myOLED(0x3C);

#define bluetooth Serial2
#define wifi Serial3

#define LOCK_CLOSING_TIMEOUT 5000

#define SERVO_1_PIN 5
#define SERVO_2_PIN 7
#define SERVO_3_PIN 8

#define COLOR_1 28, 29
#define COLOR_2 30, 31
#define COLOR_3 32, 33
#define COLOR_BACKLIGHT 27

#define RFID_TRIG_PIN 22

#define NUM_LEDS 16

Adafruit_PN532 nfc(RFID_TRIG_PIN, 100);

Adafruit_TCS34725softi2c colorSensor1 = Adafruit_TCS34725softi2c(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X, COLOR_1);
Adafruit_TCS34725softi2c colorSensor2 = Adafruit_TCS34725softi2c(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X, COLOR_2);
Adafruit_TCS34725softi2c colorSensor3 = Adafruit_TCS34725softi2c(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X, COLOR_3);

CRGB leds[NUM_LEDS];

// API Endpoints
String addTrash         = "/container/addTrash?rfid=2!";
String updateOverfilled = "/container/overfill?containerId=";
String userLogIn        = "/user/getByRfid?rfid=";

Servo lock1;
Servo lock2;
Servo lock3;

uint8_t staffCardId[7]  = {0x04, 0xDE, 0x43, 0xF2, 0xFB, 0x60, 0x80};
uint8_t personIdCard[7] = {0x04, 0xEF, 0xBB, 0xF2, 0xFB, 0x60, 0x80};

enum User {
  unknown,
  person,
  staff,
};

bool locksOpened[3] = {false, false, false};
bool locksActuallyOpened[3] = {true, true, true};
unsigned int locksOpenedAt[3] = {0, 0, 0};

bool garbageAllowed[3] = {false, false, false};
bool trashOverfill[3]  = {false, false, false};

bool personSignedIn = false;
bool lockedForMaintaining = false;

void updateColorSensorsValues() {
  bool firstSensorColorMatched  = getColorSensorValues(colorSensor1, 120, 180); // RED
  bool secondSensorColorMatched = getColorSensorValues(colorSensor2, -5, 60); // YELLOW
  bool thirdSensorColorMatched  = getColorSensorValues(colorSensor3, -50, -30); // GREEN

  Serial.print("1: ");
  Serial.print(firstSensorColorMatched);
  Serial.print(" || 2: ");
  Serial.print(secondSensorColorMatched);
  Serial.print(" || 3: ");
  Serial.println(thirdSensorColorMatched);
  
  garbageAllowed[0] = firstSensorColorMatched;
  garbageAllowed[1] = secondSensorColorMatched;
  garbageAllowed[2] = thirdSensorColorMatched;

  if (!firstSensorColorMatched && !secondSensorColorMatched && !thirdSensorColorMatched) toggleMatrix(true);
  if (firstSensorColorMatched || secondSensorColorMatched || thirdSensorColorMatched) toggleMatrix(false);
}

// TODO. Get data from distance sensors
void updateDistanceSensorsValues() {
  if (false && !trashOverfill[0]) {
    trashOverfill[0] = true;
    wifi.print(updateOverfilled + "0!");
    updateOverfillOnDisplay();
  }

  if (false && !trashOverfill[1]) {
    trashOverfill[1] = true;
    wifi.print(updateOverfilled + "1!");
    updateOverfillOnDisplay();
  }

  if (false && !trashOverfill[2]) {
    trashOverfill[2] = true;
    wifi.print(updateOverfilled + "2!");
    updateOverfillOnDisplay();
  }
}

bool compareUids(uint8_t* a, uint8_t* b) {
  bool ok = true;

  for (int i = 0; i < 4; i++) {
    if (a[i] != b[i]) ok = false;
  }
  return ok;
}

void updateLocks() {
  if (millis() - locksOpenedAt[0] > LOCK_CLOSING_TIMEOUT && locksActuallyOpened[0]) {
    locksOpened[0] = false;
    personSignedIn = false;
    updateDistanceSensorsValues();
  }

  if (millis() - locksOpenedAt[1] > LOCK_CLOSING_TIMEOUT && locksActuallyOpened[1]) {
    locksOpened[1] = false;
    personSignedIn = false;
    updateDistanceSensorsValues();
  }

  if (millis() - locksOpenedAt[2] > LOCK_CLOSING_TIMEOUT && locksActuallyOpened[2]) {
    locksOpened[2] = false;
    personSignedIn = false;
    updateDistanceSensorsValues();
  }

  if (locksOpened[0] && !locksActuallyOpened[0]) {
    locksOpenedAt[0] = millis();
    locksActuallyOpened[0] = true;
    lock1.write(90);
  } else if (!locksOpened[0] && locksActuallyOpened[0]) {
    lock1.write(0);
    locksActuallyOpened[0] = false;
  }

  if (locksOpened[1] && !locksActuallyOpened[1]) {
    locksOpenedAt[1] = millis();
    locksActuallyOpened[1] = true;
    lock2.write(90);
  } else if (!locksOpened[1] && locksActuallyOpened[1]) {
    lock2.write(0);
    locksActuallyOpened[1] = false;
  }

  if (locksOpened[2] && !locksActuallyOpened[2]) {
    locksOpenedAt[2] = millis();
    locksActuallyOpened[2] = true;
    lock3.write(90);
  } else if (!locksOpened[2] && locksActuallyOpened[2]) {
    lock3.write(0);
    locksActuallyOpened[2] = false;
  }
}

User getNfcUser() {
  User user = unknown;

  uint8_t success;
  // буфер для хранения ID карты
  uint8_t uid[8];
  // размер буфера карты
  uint8_t uidLength;
  // слушаем новые метки
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    bool ok = true;
    if (uidLength == 7) {
      if (compareUids(uid, staffCardId)) {
        user = staff;
      } else if (compareUids(uid, personIdCard)) {
        user = person;
      }
    }
    
    Serial.println("Found a card");
  }

  return user;
}

void initNFC() {
  pinMode(RFID_TRIG_PIN, INPUT);
  nfc.begin();
  
  if (!nfc.getFirmwareVersion())
  {
    Serial.print("Didn't find RFID/NFC reader");
    while (1)
    {
    }
  }

  Serial.println("Found RFID/NFC reader");
  nfc.setPassiveActivationRetries(0x11);
  nfc.SAMConfig();
}

void colorSensorsInit() {
  if (!colorSensor1.begin()) {
    Serial.println("1 Light Sensor NOT found...");
    while (1);
  }
  if (!colorSensor2.begin()) {
    Serial.println("2 Light Sensor NOT found...");
    while (1);
  }
  if (!colorSensor3.begin()) {
    Serial.println("3 Light Sensor NOT found...");
    while (1);
  }
}

bool getColorSensorValues(Adafruit_TCS34725softi2c sensor, float minD, float maxD) {
  float r, g, b;
  sensor.getRGB(&r, &g, &b);
  float delta = r - g;
  
  Serial.print("D: ");
  Serial.print(delta);
  Serial.print(" ");
  

  return (delta < maxD && delta > minD);
}

void setup() {
  Serial.begin(115200);
  pinMode(COLOR_BACKLIGHT, OUTPUT);
  digitalWrite(COLOR_BACKLIGHT, HIGH);

  wifi.begin(115200);
  lock1.attach(SERVO_1_PIN);
  lock2.attach(SERVO_2_PIN);
  lock3.attach(SERVO_3_PIN);

  initNFC();
  myOLED.begin();
  updateOverfillOnDisplay();

  FastLED.addLeds<NEOPIXEL, 23>(leds, NUM_LEDS);
  FastLED.setBrightness(20);
  FastLED.clear();
  FastLED.show();
  
  toggleMatrix(false);
  Serial.println("Init completed");
}

// TODO. Handle Bluetooth
void handleBluetooth() {

}

void toggleMatrix(bool turnedOn) {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (turnedOn) leds[i] = CRGB::Red;
    else leds[i] = CRGB::Black;
    FastLED.show();
  }
}

void showMaintainingModeOnDisplay() {
  myOLED.clearDisplay();
  myOLED.setFont(font6x8);

  myOLED.print("MAINTAINING MODE", OLED_CENTER, 30);
}

void updateOverfillOnDisplay() {
  myOLED.clearDisplay();
  myOLED.setFont(font6x8);

  myOLED.print("СONTAINERS STATUS", OLED_CENTER, 10);

  if (trashOverfill[0]) {
    myOLED.print("1 - OVERFILLED", OLED_CENTER, 20);
  } else {
    myOLED.print("1 - NORMAL", OLED_CENTER, 20);
  }

  if (trashOverfill[1]) {
    myOLED.print("2 - OVERFILLED", OLED_CENTER, 30);
  } else {
    myOLED.print("2 - NORMAL", OLED_CENTER, 30);
  }

  if (trashOverfill[2]) {
    myOLED.print("3 - OVERFILLED", OLED_CENTER, 40);
  } else {
    myOLED.print("3 - NORMAL", OLED_CENTER, 40);
  }
}

void loop() {
  User currentUser = getNfcUser();
  if (currentUser == staff) {
    wifi.print(userLogIn + "staff:");
    locksOpened[0] = true;
    locksOpened[1] = true;
    locksOpened[2] = true;

    trashOverfill[0] = false;
    trashOverfill[1] = false;
    trashOverfill[2] = false;
  } else if (currentUser == person) {
    wifi.print(userLogIn + "person:");
    personSignedIn = true;
  } else if (currentUser == unknown) wifi.print(userLogIn + "unknown:");

  if (personSignedIn) {
    Serial.println("PersonSignedIn = TRUE: Waiting for the garbage");
    updateColorSensorsValues();
    updateDistanceSensorsValues();

    if (garbageAllowed[0] && !trashOverfill[0] && !locksOpened[0]) {
      locksOpened[0] = true;
      wifi.print(addTrash);
    }

    if (garbageAllowed[1] && !trashOverfill[1] && !locksOpened[1]) {
      locksOpened[1] = true;
      wifi.print(addTrash);
    }
    
    if (garbageAllowed[2] && !trashOverfill[2] && !locksOpened[2]) {
      locksOpened[2] = true;
      wifi.print(addTrash);
    }
  } else {
    toggleMatrix(false);
  }

  if (wifi.available()) {
    bool newLockedForMaintaining = wifi.parseInt() == 1;
    if (newLockedForMaintaining != lockedForMaintaining) {
      if (newLockedForMaintaining) showMaintainingModeOnDisplay();
      else updateOverfillOnDisplay();
    }

    lockedForMaintaining = newLockedForMaintaining;
    wifi.flush();
  }

  updateLocks();
}
