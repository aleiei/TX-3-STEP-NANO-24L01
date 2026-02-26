/*
  MIT License

  Copyright (c) 2021 Alessandro Orlando

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

// Libraries
#include <Arduino.h>
#include <Bounce2.h>
#include <nRF24L01.h>
#include <RF24.h>

// Motors joystick button enable
const unsigned int pinSwEnable = 2;

// LED pin enable motors
const unsigned int ledEnable = 7;

// Radio Chip Select & Chip Enable
const unsigned int radioCE = 9;
const unsigned int radioCS = 10;

// Joystick analog pins
const unsigned int jX = A0;
const unsigned int jY = A1;
const unsigned int jZ = A2;

// FIX: avoid all-zero address — causes collisions with nRF24L01 broadcast packets.
// Must match receiver exactly.
const byte address[6] = "00001";

// Speed range
const int maxSpeed = 1000;
const int minSpeed = 0;

/*
  The reading of the potentiometers is never 100% reliable.
  This value helps determine the point to be considered as "Hold still" movement.
*/
const int threshold = 30;  // FIX: corrected typo "treshold" → "threshold"

// Milliseconds for debounce button
const unsigned long debounceDelay = 10;

// FIX: __attribute__((packed)) prevents compiler padding between bool and int16_t,
//      which would push the struct over the nRF24L01 32-byte payload limit.
// FIX: long (4 bytes) → int16_t (2 bytes) — range -1000..+1000 fits in int16_t.
//      Packed struct size: 3*(1+2) + 1 = 10 bytes. Safe and matches receiver.
struct __attribute__((packed)) Packet {
  bool    moveX;
  int16_t speedX;
  bool    moveY;
  int16_t speedY;
  bool    moveZ;
  int16_t speedZ;
  bool    enable;
};

// Bounce button instance
Bounce btnEnable = Bounce();

// Radio object
RF24 radio(radioCE, radioCS);

// Initialize packet to safe stopped state
Packet pkt = { false, 0, false, 0, false, 0, false };

// ---------------------------------------------------------------------------
// Reads joystick values, maps them and updates the Packet struct.
// FIX: variables moved from global scope to local — no need to be global.
// FIX: map() arguments were inverted in the "go ahead/up/left" branches,
//      producing wrong or negative speed values.
// ---------------------------------------------------------------------------
void handleJoystick() {

  // Analogue reading of the joystick potentiometers
  long valX = analogRead(jX);
  long valY = analogRead(jY);
  long valZ = analogRead(jZ);

  // Map raw ADC values (0–1023) to speed range (0–1000)
  long mapX = map(valX, 0, 1023, minSpeed, maxSpeed);
  long mapY = map(valY, 0, 1023, minSpeed, maxSpeed);
  long mapZ = map(valZ, 0, 1023, minSpeed, maxSpeed);

  // Threshold boundaries for deadzone
  const long thresholdDown = (maxSpeed / 2) - threshold;
  const long thresholdUp   = (maxSpeed / 2) + threshold;

  // --- Axis X ---
  if (mapX <= thresholdDown) {
    // X goes back — negative speed
    // FIX: was map(mapX, tresholdDown, minSpeed, ...) → fromLow > fromHigh, wrong result
    pkt.speedX = (int16_t)(-map(mapX, minSpeed, thresholdDown, minSpeed, maxSpeed));
    pkt.moveX  = true;
  } else if (mapX >= thresholdUp) {
    // X goes forward — positive speed
    // FIX: was map(mapX, maxSpeed, tresholdUp, ...) → fromLow > fromHigh, wrong result
    pkt.speedX = (int16_t)(map(mapX, thresholdUp, maxSpeed, minSpeed, maxSpeed));
    pkt.moveX  = true;
  } else {
    // X deadzone — stand still
    pkt.speedX = 0;
    pkt.moveX  = false;
  }

  // --- Axis Y ---
  if (mapY <= thresholdDown) {
    pkt.speedY = (int16_t)(-map(mapY, minSpeed, thresholdDown, minSpeed, maxSpeed));
    pkt.moveY  = true;
  } else if (mapY >= thresholdUp) {
    pkt.speedY = (int16_t)(map(mapY, thresholdUp, maxSpeed, minSpeed, maxSpeed));
    pkt.moveY  = true;
  } else {
    pkt.speedY = 0;
    pkt.moveY  = false;
  }

  // --- Axis Z ---
  if (mapZ <= thresholdDown) {
    pkt.speedZ = (int16_t)(-map(mapZ, minSpeed, thresholdDown, minSpeed, maxSpeed));
    pkt.moveZ  = true;
  } else if (mapZ >= thresholdUp) {
    pkt.speedZ = (int16_t)(map(mapZ, thresholdUp, maxSpeed, minSpeed, maxSpeed));
    pkt.moveZ  = true;
  } else {
    pkt.speedZ = 0;
    pkt.moveZ  = false;
  }
}

// ---------------------------------------------------------------------------
// Reads the enable button state and updates the LED.
// ---------------------------------------------------------------------------
void handleButton() {
  btnEnable.update();
  if (btnEnable.fell()) {
    pkt.enable = !pkt.enable;
  }
  digitalWrite(ledEnable, pkt.enable);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Button and LED pins
  pinMode(pinSwEnable, INPUT_PULLUP);
  pinMode(ledEnable,   OUTPUT);

  // Radio initialization
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(address);
  radio.stopListening();

  // Debounce button
  btnEnable.attach(pinSwEnable);
  btnEnable.interval(debounceDelay);

  // Initial LED state
  digitalWrite(ledEnable, pkt.enable);

  // Debug: confirm packed struct fits within nRF24L01 32-byte limit
  Serial.print("Packet size: ");
  Serial.print(sizeof(pkt));
  Serial.println(" bytes (max 32)");
}

// ---------------------------------------------------------------------------
void loop() {
  handleButton();
  handleJoystick();

  // FIX: check radio.write() return value — false means transmission failed
  bool ok = radio.write(&pkt, sizeof(pkt));
  if (!ok) {
    Serial.println("TX failed");
  }
}