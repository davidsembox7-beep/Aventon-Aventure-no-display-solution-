// Aventon Aventure.2 - run the stock controller with no display, over Bluetooth LE.
//
// Advertises as "AventonBike". A phone connects over BLE, sends a passcode,
// then controls assist level, lights and walk assist. The board keeps the
// controller link alive at 9600 baud and reports wheel speed back.
//
// BLE does not touch WiFi or mobile data, so nothing has to be switched off
// on the phone.
//
// ---------------------------------------------------------------------------
// PROTOCOL (reverse engineered on an Aventon Aventure.2, all bits verified)
//
//   KingMeter 618U style, 9600 baud, 8N1, on Serial1 (D0/D1)
//   Display -> controller, 6 bytes exactly:
//
//       46  <flags>  20  00  00  0D
//
//     flags bits 0-3 = assist level 0-5   (affects PEDAL assist only)
//     flags bit  4   = walk assist        (fixed slow speed, no throttle needed)
//     flags bit  7   = lights             (front and rear together)
//
//   Controller -> display, 8 bytes:
//
//       46  03  <current>  <speed hi>  <speed lo>  00  <x>  <x>
//
//     speed is a wheel period: LOWER means FASTER. 3341 (0x0D0D) = stopped.
//
//   Message length is exactly 6 bytes. 7 or more gets no reply at all.
//
//   The throttle works whenever this link is being held up, at any assist
//   level including 0. Assist level only changes pedal assist.
// ---------------------------------------------------------------------------
//
// WIRING (8-pin Julet at the controller, display branch):
//   D0 (RX)  -> display wire 1
//   D1 (TX)  -> display wire 2
//   GND      -> display GND        <-- REQUIRED. See README.
//
// SPDX-License-Identifier: MIT

#include <ArduinoBLE.h>

// ---------------------------------------------------------------------------
// CHANGE THIS before flashing. Anything up to 15 characters.
// Without the right passcode the board ignores every command, so a stranger
// in Bluetooth range cannot control your bike.
const char* PASSCODE = "CHANGE_ME";
// ---------------------------------------------------------------------------

BLEService bikeService("19b10000-e8f2-537e-4f6c-d104768a1214");

BLEByteCharacteristic  controlChar("19b10001-e8f2-537e-4f6c-d104768a1214",
                                   BLERead | BLEWrite);
BLECharacteristic      statusChar ("19b10002-e8f2-537e-4f6c-d104768a1214",
                                   BLERead | BLENotify, 4);
BLECharacteristic      authChar   ("19b10003-e8f2-537e-4f6c-d104768a1214",
                                   BLEWrite, 16);

byte flagsByte = 0x00;
bool authed    = false;

int  lastSpeed   = 3341;
int  lastCurrent = 0;
bool linkOk      = false;

unsigned long lastSend   = 0;
unsigned long lastNotify = 0;
const unsigned long SEND_INTERVAL = 150;

void sendFrame() {
  byte frame[6] = {0x46, flagsByte, 0x20, 0x00, 0x00, 0x0D};

  while (Serial1.available()) Serial1.read();
  Serial1.write(frame, 6);
  Serial1.flush();

  unsigned long t = millis();
  byte rx[16];
  byte n = 0;
  while (millis() - t < 60 && n < 16) {
    if (Serial1.available()) rx[n++] = Serial1.read();
  }

  if (n >= 5) {
    linkOk      = true;
    lastCurrent = rx[2];
    lastSpeed   = (int)rx[3] * 256 + (int)rx[4];
  } else {
    linkOk = false;
  }
}

void pushStatus() {
  byte s[4];
  s[0] = (linkOk ? 1 : 0) | (authed ? 2 : 0);
  s[1] = (byte)lastCurrent;
  s[2] = (byte)(lastSpeed >> 8);
  s[3] = (byte)(lastSpeed & 0xFF);
  statusChar.writeValue(s, 4);
}

void checkAuth() {
  char given[17];
  int len = authChar.valueLength();
  if (len > 16) len = 16;
  memcpy(given, authChar.value(), len);
  given[len] = '\0';

  authed = (strcmp(given, PASSCODE) == 0);

  if (!authed) {
    flagsByte = 0x00;   // reject: make sure nothing is left running
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  if (!BLE.begin()) {
    while (1) {
      Serial.println("BLE FAILED TO START");
      delay(1000);
    }
  }

  BLE.setLocalName("AventonBike");
  BLE.setDeviceName("AventonBike");
  BLE.setAdvertisedService(bikeService);

  bikeService.addCharacteristic(controlChar);
  bikeService.addCharacteristic(statusChar);
  bikeService.addCharacteristic(authChar);
  BLE.addService(bikeService);

  controlChar.writeValue(0);
  pushStatus();

  BLE.advertise();
  Serial.println("BLE advertising as AventonBike");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    authed    = false;
    flagsByte = 0x00;

    while (central.connected()) {
      if (authChar.written()) checkAuth();

      if (controlChar.written()) {
        if (authed) {
          flagsByte = controlChar.value();
        } else {
          flagsByte = 0x00;   // not authenticated: ignore
        }
      }

      if (millis() - lastSend >= SEND_INTERVAL) {
        lastSend = millis();
        sendFrame();
      }

      if (millis() - lastNotify >= 500) {
        lastNotify = millis();
        pushStatus();
      }
    }

    // Phone went away: drop assist.
    authed    = false;
    flagsByte = 0x00;
  }

  // Keep the controller link alive even with no phone connected.
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    sendFrame();
  }
}
