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
#include <Adafruit_PN532.h>
#include <Servo.h>
#include <TroykaOLED.h>
TroykaOLED display(0x3C);

#define bluetooth Serial2
#define wifi Serial3

#define LOCK_CLOSING_TIMEOUT 5000

#define SERVO_1_PIN 5
#define SERVO_2_PIN 7
#define SERVO_3_PIN 8

#define RFID_TRIG_PIN 22

Adafruit_PN532 nfc(RFID_TRIG_PIN, 100);

// API Endpoints
String addTrash         = "/container/addTrash?rfid=2!";
String updateOverfilled = "/container/overfill?containerId=";
String userLogIn        = "/user/getByRfid?rfid=";

Servo lock1;
Servo lock2;
Servo lock3;

uint8_t staffCardId[4]  = {0x89, 0x54, 0xA8, 0x99}; // TODO.
uint8_t personIdCard[4] = {0x89, 0x54, 0xA8, 0x99}; // TODO.

enum User {
  unknown,
  person,
  staff,
};

bool locksOpened[3] = {false, false, false};
unsigned int locksOpenedAt[3] = {0, 0, 0};

bool garbageAllowed[3] = {false, false, false};
bool trashOverfill[3]  = {false, false, false};

bool personSignedIn = false;
bool lockedForMaintaining = false;

// TODO. Get data from color sensors
void updateColorSensorsValues() {
  if (true) garbageAllowed[0] = true;
  if (true) garbageAllowed[1] = true;
  if (true) garbageAllowed[2] = true;
}

// TODO. Get data from light sensors
void updateLightSensorsValues() {
  if (true && !trashOverfill[0]) {
    trashOverfill[0] = true;
    wifi.print(updateOverfilled + "0!");
    updateOverfillOnDisplay();
  }

  if (true && !trashOverfill[1]) {
    trashOverfill[1] = true;
    wifi.print(updateOverfilled + "1!");
    updateOverfillOnDisplay();
  }

  if (true && !trashOverfill[2]) {
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
  if (millis() - locksOpenedAt[0] > LOCK_CLOSING_TIMEOUT && locksOpened[0]) {
    locksOpened[0] = false;
    personSignedIn = false;
    updateLightSensorsValues();
  }

  if (millis() - locksOpenedAt[1] > LOCK_CLOSING_TIMEOUT && locksOpened[1]) {
    locksOpened[1] = false;
    personSignedIn = false;
    updateLightSensorsValues();
  }

  if (millis() - locksOpenedAt[2] > LOCK_CLOSING_TIMEOUT && locksOpened[2]) {
    locksOpened[2] = false;
    personSignedIn = false;
    updateLightSensorsValues();
  }

  if (locksOpened[0]) {
    locksOpenedAt[0] = millis();
    lock1.write(90);
  } else lock1.write(0);

  if (locksOpened[1]) {
    locksOpenedAt[1] = millis();
    lock2.write(90);
  } else lock2.write(0);

  if (locksOpened[2]) {
    locksOpenedAt[2] = millis();
    lock3.write(90);
  } else lock3.write(0);
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
    Serial.println("Found a card");
    Serial.print("ID Length: ");
    Serial.print(uidLength, DEC);
    Serial.println(" bytes");
    Serial.print("ID Value: ");
    nfc.PrintHex(uid, uidLength);

    bool ok = true;
    // TODO.         ||
    if (uidLength == 4) {
      if (compareUids(uid, staffCardId)) {
        user = staff;
      } else if (compareUids(uid, personIdCard)) {
        user = person;
      }
    }
    
    Serial.println("Found a card");
    Serial.print("ID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
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

void setup() {
  Serial.begin(9600);

  wifi.begin(11200);
  lock1.attach(SERVO_1_PIN);
  lock2.attach(SERVO_2_PIN);
  lock3.attach(SERVO_3_PIN);

  initNFC();
  display.begin();
}

// TODO. Handle Bluetooth
void handleBluetooth() {

}

void showMaintainingModeOnDisplay() {
  display.clearDisplay();
  display.setFont(font6x8);

  display.print("MAINTAINING MODE", OLED_CENTER, 30);
}

void updateOverfillOnDisplay() {
  display.clearDisplay();
  display.setFont(font6x8);

  display.print("СONTAINERS STATUS", OLED_CENTER, 10);
  if (trashOverfill[0]) {
    display.print("1 - OVERFILLED", OLED_CENTER, 20);
  } else {
    display.print("1 - NORMAL", OLED_CENTER, 20);
  }

  if (trashOverfill[1]) {
    display.print("2 - OVERFILLED", OLED_CENTER, 20);
  } else {
    display.print("2 - NORMAL", OLED_CENTER, 20);
  }

  if (trashOverfill[2]) {
    display.print("3 - OVERFILLED", OLED_CENTER, 20);
  } else {
    display.print("3 - NORMAL", OLED_CENTER, 20);
  }
}

void loop() {
  User currentUser = getNfcUser();
  if (currentUser == staff) {
    wifi.print(userLogIn + "1:");
    locksOpened[0] = true;
    locksOpened[1] = true;
    locksOpened[2] = true;
  } else if (currentUser == person) {
    wifi.print(userLogIn + "2:");
    personSignedIn = true;
  } else if (currentUser == unknown) wifi.print(userLogIn + "3:");

  if (personSignedIn) {
    Serial.println("PersonSignedIn = TRUE: Waiting for the garbage");
    updateColorSensorsValues();
    updateLightSensorsValues();

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