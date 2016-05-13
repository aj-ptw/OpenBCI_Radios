/*

Sets up RFduino Device for OpenBCI_32bit using RFduinoGZLL library

This test behaves as a serial pass thru between two RFduinos,
To Program, user must have RFduino core files installed in Arduino 1.5.4 or later
Use the RFRST, RFRX, RFTX, and GND on the board to connect to USB<>Serial device
Your USB<>Serial device must connect RFRST with DTR through 0.1uF capacitor (sorry)

This code uses excessive buffering of Serial and Radio packets
2D arrays, and ring buffers are used, I think, when streaming data
Also uses using timout to find end of serial data package

Made by AJ Keller, Spring 2016
Free to use and share. This code presented as-is. No promises!
o
*/
#include <RFduinoGZLL.h>
#include "OpenBCI_Radios.h"

void setup() {
  // put your setup code here, to run once:
  radio.begin(OPENBCI_MODE_DEVICE,20);
}

void loop() {
  // put your main code here, to run repeatedly:

  if (radio.didPicSendDeviceSerialData()) {
    radio.getSerialDataFromPicAndPutItInTheDevicesSerialBuffer();
  }

  if (radio.thereIsDataInSerialBuffer()) {
    if (radio.theLastTimeNewSerialDataWasAvailableWasLongEnough()){
      radio.sendTheDevicesFirstPacketToTheHost();
  }
  }

  if (radio.isTheDevicesRadioBufferFilledWithAllThePacketsFromTheHost) {
    radio.writeTheDevicesRadioBufferToThePic();
  }

  if (radio.isAStreamPacketWaitingForLaunch()) {
    if (radio.hasEnoughTimePassedToLaunchStreamPacket()) {
      radio.sendStreamPacketToTheHost();
    }
  }
}