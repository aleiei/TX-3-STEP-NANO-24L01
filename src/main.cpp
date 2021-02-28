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

//Library
#include <Arduino.h>
#include <Bounce2.h>
#include <nRF24L01.h>
#include <RF24_config.h>
#include <RF24.h>

//Motors joystick button enable
const unsigned int pinSwEnable = 2;

//Led pin enable motors
const unsigned int ledEnable = 7;

//Radio Chip Select & Chip Enable
const unsigned int radioCE = 9;
const unsigned int radioCS = 10;

//joystick analog pins
const unsigned int jX = A0;
const unsigned int jY = A1;
const unsigned int jZ = A2;

//Radio address
const byte address[5] = {0, 0, 0, 0, 0};

//Variables used to define min and max speed and map to joystick values
const unsigned int maxSpeed = 1000;
const unsigned int minSpeed = 0;

/*
  The reading of the potentiometers is never 100% reliable
  This value helps determine the point to be considered as "Hold still" movement
*/
const unsigned int treshold = 30;

//Milliseconds for debonuce Button
const unsigned long debounceDelay = 10;

//Package structure to be sent
struct Packet {
  boolean moveX;
  long speedX;
  boolean moveY;
  long speedY;
  boolean moveZ;
  long speedZ;
  boolean enable;
};

//Variables
long  valX, mapX, valY, mapY, valZ, mapZ, tresholdUp, tresholdDown;

//Button from the Bounce library
Bounce btnEnable = Bounce();

//Radio pins connected to CE and CSN of the module
RF24 radio(radioCE, radioCS);

//Initialize packet to send
Packet pkt = {
  //boolean moveX;
  false,
  //long speedX;
  0,
  //boolean moveY;
  false,
  //long speedY;
  0,
  //boolean moveZ;
  false,
  //long speedZ;
  0,
  //boolean enable button;
  false,
};

void setup() {

  //Enable pullup button pin
  pinMode(pinSwEnable, INPUT_PULLUP);

  //Enable LED pin
  pinMode(ledEnable, OUTPUT);

  //Radio inizialize 
  radio.begin();
 
  //Radio output power, in my case at LOW
  radio.setPALevel(RF24_PA_LOW);

  //Radio communication channel on the specified address (it will be the same for the receiver)
  radio.openWritingPipe(address);

  //The radio does not receive, it only transmits
  radio.stopListening();

  //Button instance
  btnEnable.attach(pinSwEnable);
  btnEnable.interval(debounceDelay);

   //Range of values within which to consider the position of the joystick as "Stay still"
  tresholdDown = (maxSpeed / 2) - treshold;
  tresholdUp = (maxSpeed / 2) + treshold;

  //LED enable status
  digitalWrite(ledEnable, pkt.enable);
}

//Reads the joystick values, maps them and updates the variables in the Packet
void handleJoystick() {

  //Analogue reading of the values coming from the joystick potentiometers
  valX = analogRead(jX);
  valY = analogRead(jY);
  valZ = analogRead(jZ);

  //Map of values read in accordance with the minimum and maximum speed
  mapX = map(valX, 0, 1023, minSpeed, maxSpeed);
  mapY = map(valY, 0, 1023, minSpeed, maxSpeed);
  mapZ = map(valZ, 0, 1023, minSpeed, maxSpeed);

  if (mapX <= tresholdDown) {
    //x goes back
    pkt.speedX = -map(mapX, tresholdDown, minSpeed,   minSpeed, maxSpeed);
    pkt.moveX = true;
  } else if (mapX >= tresholdUp) {
    //x go ahead
    pkt.speedX = map(mapX,  maxSpeed, tresholdUp,  maxSpeed, minSpeed);
    pkt.moveX = true;
  } else {
    //x stands still
    pkt.speedX = 0;
    pkt.moveX = false;
  }

  if (mapY <= tresholdDown) {
    //y go down
    pkt.speedY = -map(mapY, tresholdDown, minSpeed,   minSpeed, maxSpeed);
    pkt.moveY = true;
  } else if (mapY >= tresholdUp) {
    //y go up
    pkt.speedY = map(mapY,  maxSpeed, tresholdUp,  maxSpeed, minSpeed);
    pkt.moveY = true;
  } else {
    //y stands still
    pkt.speedY = 0;
    pkt.moveY = false;
  }

    if (mapZ <= tresholdDown) {
    //z goes right
    pkt.speedZ = -map(mapZ, tresholdDown, minSpeed,   minSpeed, maxSpeed);
    pkt.moveZ = true;
  } else if (mapZ >= tresholdUp) {
    //z goes left
    pkt.speedZ = map(mapZ,  maxSpeed, tresholdUp,  maxSpeed, minSpeed);
    pkt.moveZ = true;
  } else {
    //z stands still
    pkt.speedZ = 0;
    pkt.moveZ = false;
  }

}

//reads the state of the enable button
void handleButton() {

  btnEnable.update();
  if (btnEnable.fell()) {
    pkt.enable = !pkt.enable;
  }

  //Show enable status with LED
  digitalWrite(ledEnable, pkt.enable);

  }
  void loop() {

  //Button status loop
  handleButton();

  //Joystick status loop
  handleJoystick();

  //Radio packet send loop
  radio.write(&pkt, sizeof(pkt));

}