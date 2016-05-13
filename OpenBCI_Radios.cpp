/***************************************************
This is a library for the OpenBCI 32bit RFduinoGZLL Device and host
Let us define two over arching operating modes / paradigms: Host and Device, where:
* Host is connected to PC via USB VCP (FTDI).
* Device is connectedd to uC (PIC32MX250F128B with UDB32-MX2-DIP).

These displays use I2C to communicate, 2 pins are required to
interface
OpenBCI invests time and resources providing this open source code,
please support OpenBCI and open-source hardware by purchasing
products from OpenBCI!

Written by AJ Keller of Push The World LLC but much credit must also go to
Joel Murphy who with Conor Russomanno and Leif Percifield created the
original OpenBCI_32bit_Device.ino and OpenBCI_32bit_Host.ino files in the
Summer of 2014. Much of this code base is inspired directly from their work.

MIT license
****************************************************/

#include "OpenBCI_Radios.h"

// CONSTRUCTOR
OpenBCI_Radios_Class::OpenBCI_Radios_Class() {
    // Set defaults
    radioMode = OPENBCI_MODE_DEVICE; // Device mode
    radioChannel = 25; // Channel 18
    verbosePrintouts = true;
    debugMode = true; // Set true if doing dongle-dongle sim
    isHost = false;
    isDevice = false;
}

/**
* @description The function that the radio will call in setup()
* @param: mode [unint8_t] - The mode the radio shall operate in
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::begin(uint8_t mode) {
    // Save global radio mode
    radioMode = mode;

    // configure radio
    configure(mode,radioChannel);

}

/**
* @description The function that the radio will call in setup()
* @param: mode [unint8_t] - The mode the radio shall operate in
* @param: channelNumber [int8_t] - The channelNumber the RFduinoGZLL will
*           use to communicate with the other RFduinoGZLL.
*           NOTE: Must be from 0 - 25
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::begin(uint8_t mode, uint32_t channelNumber) {
    // Save global radio mode
    radioMode = mode;
    // Restrict the channel to 0-25 inclusively
    if (channelNumber > RFDUINOGZLL_CHANNEL_LIMIT_UPPER || channelNumber < RFDUINOGZLL_CHANNEL_LIMIT_LOWER) {
        channelNumber = RFDUINOGZLL_CHANNEL_LIMIT_LOWER;
    }

    // configure radio
    configure(mode,channelNumber);

}

/**
* @description Private function to initialize the OpenBCI_Radios_Class object
* @param: mode [unint8_t] - The mode the radio shall operate in
* @param: channelNumber [int8_t] - The channelNumber the RFduinoGZLL will
*           use to communicate with the other RFduinoGZLL
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::configure(uint8_t mode, uint32_t channelNumber) {
    // Quickly check to see if in pass through mode, if so, call and dip out of func
    if (mode == OPENBCI_MODE_PASS_THRU) {
        configurePassThru();
    } else { // we are either dealing with a Host or a Device
        // We give the oppertunity to call any 'universal' code, rather code, that
        //    gets set up the same on both Host and Device

        // Check to see if we need to set the channel number
        //  this is only the case on the first run of the program
        if (needToSetChannelNumber()) {
            setChannelNumber(channelNumber);
        }
        RFduinoGZLL.channel = getChannelNumber();
        if (isHost) {
            Serial.print("Channel: "); Serial.write(RFduinoGZLL.channel); Serial.println();
        }
        radioChannel = getChannelNumber();

        // get the buffers ready
        bufferCleanRadio();
        bufferCleanSerial(OPENBCI_MAX_NUMBER_OF_BUFFERS);

        // Diverge program execution based on Device or Host
        switch (mode) {
            case OPENBCI_MODE_DEVICE:
            configureDevice(); // setup for Device
            break;
            default: // we default to OPENBCI_MODE_HOST
            configureHost(); // setup for Host
            break;
        }
    }
}

/**
* @description Private function to initialize the radio in Device mode
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::configureDevice(void) {
    // Start the RFduinoGZLL in DEVICE0 mode
    RFduinoGZLL.begin(RFDUINOGZLL_ROLE_DEVICE);

    // Configure pins

    if (debugMode) { // Dongle to Dongle debug mode
        // BEGIN: To run host as device
        pinMode(OPENBCI_PIN_HOST_RESET,INPUT);
        pinMode(OPENBCI_PIN_HOST_LED,OUTPUT);
        digitalWrite(OPENBCI_PIN_HOST_LED,HIGH);
        Serial.begin(115200);
        // END: To run host as device
    } else {
        // BEGIN: To run host normally
        pinMode(OPENBCI_PIN_DEVICE_PCG, INPUT); //feel the state of the PIC with this pin
        // Start the serial connection. On the device we must specify which pins are
        //    rx and tx, where:
        //      rx = GPIO3
        //      tx = GPIO2
        Serial.begin(115200, 3, 2);
        // END: To run host normally
    }

    // Configure booleans
    isDevice = true;

    bufferResetStreamPacketBuffer();

    // TODO: Implement init hand shaking
    // char msg[1];
    // msg[0] = ORPM_PACKET_INIT & 0xFF;
    // RFduinoGZLL.sendToHost(msg,1);
    pollRefresh();

}

/**
* @description Private function to initialize the radio in Host mode
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::configureHost(void) {
    // Start the RFduinoGZLL in HOST mode
    RFduinoGZLL.begin(RFDUINOGZLL_ROLE_HOST);

    // Configure pins
    pinMode(OPENBCI_PIN_HOST_RESET,INPUT);
    pinMode(OPENBCI_PIN_HOST_LED,OUTPUT);

    // Turn LED on
    digitalWrite(OPENBCI_PIN_HOST_LED,HIGH);

    // Open the Serial connection
    Serial.begin(115200);

    isHost = true;

    bufferCleanStreamPackets(OPENBCI_MAX_NUMBER_OF_BUFFERS);

    if (verbosePrintouts) {
        Serial.println("Host radio up");
    }
}

/**
* @description Private function to initialize the radio in Pass Through mode
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::configurePassThru(void) {
    // Configure the pins
    pinMode(0,OUTPUT_D0H1);  // output is highZ when logic 0, HIGH when logic 1
    pinMode(1,OUTPUT_D0H1);
    pinMode(OPENBCI_PIN_HOST_LED,OUTPUT);

    digitalWrite(0,LOW);
    digitalWrite(1,LOW);
}

/**
 * @description Returns the channel number from non-volitile flash memory
 */
uint32_t OpenBCI_Radios_Class::getChannelNumber(void) {
    return *ADDRESS_OF_PAGE(RFDUINOGZLL_FLASH_MEM_ADDR);
}

/**
 * @description Returns the channel number from non-volitile flash memory
 */
boolean OpenBCI_Radios_Class::needToSetChannelNumber(void) {
    return getChannelNumber() == 0xFFFFFFFF;
}

void OpenBCI_Radios_Class::revertToPreviousChannelNumber(void) {
    RFduinoGZLL.channel = previousRadioChannel;
}

boolean OpenBCI_Radios_Class::setChannelNumber(uint32_t channelNumber) {
    if (channelNumber > RFDUINOGZLL_CHANNEL_LIMIT_UPPER) {
        channelNumber = RFDUINOGZLL_CHANNEL_LIMIT_UPPER;
    }
    uint32_t *p = ADDRESS_OF_PAGE(RFDUINOGZLL_FLASH_MEM_ADDR);

    int rc = flashPageErase(PAGE_FROM_ADDRESS(p));
    if (rc == 1) {
        if (isHost) {
            Serial.println("Error - the flash page is reserved$$$");
        }
        return false;
    } else if (rc == 2) {
        if (isHost) {
            Serial.println("Error - the flash page is used by the sketch$$$");
        }
        return false;
    }


    rc = flashWrite(p, channelNumber);
    if (rc == 0) {
        Serial.println("Channel Number Set$$$");
        return true;
    } else if (rc == 1) {
        if (isHost) {
            Serial.println("Error - the flash page is reserved$$$");
        }
        return false;
    } else if (rc == 2) {
        if (isHost) {
            Serial.println("Error - the flash page is used by the sketch$$$");
        }
        return false;
    }
}



/********************************************/
/********************************************/
/************    HOST CODE    ***************/
/********************************************/
/********************************************/


/**
* @description Private function to handle a request to read serial as a host
* @return boolean - TRUE if there is data to read! FALSE if not...
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::didPCSendDataToHost(void) {
    if (Serial.available() > 0) {
        return true;
    } else {
        return false;
    }
}

/**
* @description Private function to read data from serial port and write into
*                 the bufferSerial. The important thing to note here is that
*                 this function stores the incoming data into groups of 32 for
*                 O(1) time to send data over the radio
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::getSerialDataFromPCAndPutItInHostsSerialBuffer(void) {

    // Get everything from the serial port and store to bufferSerial
    bufferSerialFetch();

    lastTimeHostHeardFromDevice = millis();

}

// boolean OpenBCI_Radios_Class::isTheHostsRadioBufferFilledWithAllThePacketsFromTheDevice(void) {
//     // Check to see if we got all the packets
//     if (bufferPacketsToReceive == bufferPacketsReceived) {
//         return true;
//     } else {
//         return false;
//     }
// }

/**
* @description Called when readRadio returns true. We always write the contents
*                 of bufferRadio over Serial.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::writeTheHostsRadioBufferToThePC(void) {
    if (debugMode) {
        for (int j = 0; j < bufferPositionWriteRadio; j++) {
            Serial.print(bufferRadio[j]);
        }
        Serial.println();
    } else {
        for (int j = 0; j < bufferPositionWriteRadio; j++) {
            Serial.write(bufferRadio[j]);
        }
    }
    bufferCleanRadio();
}

/**
 * @description The first line of defense against a system that has lost it's device
 */
boolean OpenBCI_Radios_Class::hasItBeenTooLongSinceHostHeardFromDevice(void) {
    if (millis() > lastTimeHostHeardFromDevice + (OPENBCI_POLL_TIME_DURATION_MS * 2)) {
        return true;
    } else {
        return false;
    }
}

/**
 * @description If the Host received a stream packet, then this will have data in it
 *      and number of packets to send will be greater than 0
 * @return {boolean} - If there are packets to send
 */
boolean OpenBCI_Radios_Class::doesTheHostHaveAStreamPacketToSendToPC(void) {
    return bufferStreamPackets.numberOfPacketsToSend > 0;
}

/**
 * @description We now write out the stream packet buffer to the PC. It's important we place
 *      this into a buffer, and not try to write to the serial port in on_recieve because
 *      that will only lead to problems.
 */
void OpenBCI_Radios_Class::writeTheHostsStreamPacketBufferToThePC(void) {
    // Write packets until we have sent them all
    while (bufferStreamPackets.numberOfPacketsToSend > bufferStreamPackets.numberOfPacketsSent) {

        // Send first buffer out... first call would be 0th packet then 1st, and so on
        writeStreamPacket(bufferStreamPackets.packetBuffer->data + bufferStreamPackets.numberOfPacketsSent);


        // Increment the number of packets we wrote out the serial port
        bufferStreamPackets.numberOfPacketsSent++;
    }

    // Clean the bufferStreamPackets
    bufferCleanStreamPackets(bufferStreamPackets.numberOfPacketsToSend);
}

/**
* @description Sends a data of length 31 to the board in OpenBCI V3 data format
* @param data [char *] The 32 byte packet buffer
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::writeStreamPacket(char *data) {

    // Write the start byte
    Serial.write(OPENBCI_STREAM_BYTE_START);

    // Write the data
    int positionToStopReading = OPENBCI_MAX_DATA_BYTES_IN_PACKET + 1; // because byteId
    for (int i = 1; i < positionToStopReading; i++) {
        Serial.write(data[i]);
    }

    // Write the stop byte
    Serial.write(outputGetStopByteFromByteId(data[0]));
}

/********************************************/
/********************************************/
/***********    DEVICE CODE    **************/
/********************************************/
/********************************************/

/**
* @description Private function to handle a request to read serial as a device
* @return Returns TRUE if there is data to read! FALSE if not...
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::didPicSendDeviceSerialData(void) {
    if (Serial.available() > 0) {
        // Serial.print("Pic sent device serial data of length ");
        // Serial.println(Serial.available());
        return true;
    } else {
        if (pollNow()) {
            pollHost();
        }
        return false;
    }
}

/**
* @description Moves data from the Pic to the devices Serial buffer (bufferSerial)
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::getSerialDataFromPicAndPutItInTheDevicesSerialBuffer(void) {
    // Get everything from the serial port and store to bufferSerial
    bufferSerialFetch();
}

boolean OpenBCI_Radios_Class::thereIsDataInSerialBuffer(void) {
    if (bufferSerial.numberOfPacketsSent < bufferSerial.numberOfPacketsToSend) {
        return true;
    } else {
        return false;
    }
}

/**
* @description Checks to see the last time serial data was written from the Pic
*               to the Device over serial was longer then the alloted time
* @returns true if yes, and false if no
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::theLastTimeNewSerialDataWasAvailableWasLongEnough(void) {
    if (millis() > lastTimeNewSerialDataWasAvailable +  OPENBCI_MAX_SERIAL_TIMEOUT_MS) {
        return true;
    } else {
        return false;
    }
}

/**
* @description Private function to take data from the serial port and send it
*                 to the HOST
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::sendTheDevicesFirstPacketToTheHost(void) {

    // Is there data in bufferSerial?
    if (bufferSerial.numberOfPacketsToSend > 0 && bufferSerial.numberOfPacketsSent == 0) {
        // Get the packet number or type, based on streaming or not
        int packetNumber = bufferSerial.numberOfPacketsToSend - 1;

        char byteId = byteIdMake(false,packetNumber,bufferSerial.packetBuffer->data + 1, bufferSerial.packetBuffer->positionWrite - 1);

        // Add the byteId to the packet
        bufferSerial.packetBuffer->data[0] = byteId;

        // TODO: REMOVE THIS SHIT!
        if (bufferSerial.packetBuffer->data[1] == 0x00) {
            Serial.write(getChannelNumber());
        }
        // Send back some data
        RFduinoGZLL.sendToHost(bufferSerial.packetBuffer->data, bufferSerial.packetBuffer->positionWrite);

        // We know we just sent the first packet
        bufferSerial.numberOfPacketsSent = 1;

        pollRefresh();

        if (verbosePrintouts) {
            Serial.print("Si->"); Serial.print(packetNumber); Serial.print(":"); Serial.println(bufferSerial.packetBuffer->positionWrite);
        }
    }
}

/**
 * @description Checks to see if we have a packet waiting in the streamPacketBuffer
 *  waiting to be sent out
 * @return {bool} if we have a packet waiting
 */
boolean OpenBCI_Radios_Class::isAStreamPacketWaitingForLaunch(void) {
    return streamPacketBuffer.readyForLaunch;
}

/**
 * @description This checks to see if 100uS has passed since the time we got a
 *  tail packet.
 */
boolean OpenBCI_Radios_Class::hasEnoughTimePassedToLaunchStreamPacket(void) {
    return micros() > (timeWeGot0xFXFromPic +  OPENBCI_SERIAL_TIMEOUT_uS);
}

/**
 * @description Sends the contents of the `streamPacketBuffer`
 *                 to the HOST, sends as stream
 *
 * @author AJ Keller (@pushtheworldllc)
 */
void OpenBCI_Radios_Class::sendStreamPacketToTheHost(void) {

    byte packetType = byteIdMakeStreamPacketType();

    char byteId = byteIdMake(true,packetType,streamPacketBuffer.data + 1, OPENBCI_MAX_DATA_BYTES_IN_PACKET); // 31 bytes

    // Add the byteId to the packet
    streamPacketBuffer.data[0] = byteId;

    RFduinoGZLL.sendToHost(streamPacketBuffer.data, OPENBCI_MAX_PACKET_SIZE_BYTES); // 32 bytes

    // Clean the serial buffer (because these bytes got added to it too)
    bufferCleanSerial(bufferSerial.numberOfPacketsToSend);

    // Clean the stream packet buffer
    bufferResetStreamPacketBuffer();

    // Refresh the poll timeout timer because we just polled the Host by sending
    //  that last packet
    pollRefresh();
}

/**
 * @description Called ever time there is a new byte read in on a device
 */
void OpenBCI_Radios_Class::processCharForStreamPacket(char newChar) {
    // A stream packet comes in as 'A'|data|0xFX where X can be 0-15
    // Are we currently looking for 0xF0
    if (streamPacketBuffer.readyForLaunch) {
        // Serial.println("ABORT!");
        // We just got another char and we were ready for launch... well abort
        //  because this is an OTA program or something.. something else
        bufferResetStreamPacketBuffer();
    } else if (streamPacketBuffer.gotHead) {
        // Store in the special stream buffer
        streamPacketBuffer.data[streamPacketBuffer.bytesIn] = newChar;

        // Increment the number of bytes in for this possible stream packet
        streamPacketBuffer.bytesIn++;

        // Serial.print("NRML: "); Serial.print(streamPacketBuffer.bytesIn); Serial.println(newChar);
        // Have we read in the number of data bytes in a stream packet?
        if (streamPacketBuffer.bytesIn == OPENBCI_MAX_PACKET_SIZE_BYTES + 1) {
            // Check to see if this character is a type, 0xFx where x is 0-15
            if ((newChar & 0xF0) == 0xF0) {
                // Serial.println("Tail test pass!");
                // Save this char as the type, will come in handy in the
                //  next step of sending a packet
                streamPacketBuffer.typeByte = newChar;

                // Mark this as the time we got a serial buffer
                timeWeGot0xFXFromPic = micros();

                // mark ready for launch
                streamPacketBuffer.readyForLaunch = true;
            } else {
                // Serial.println("Tail test fail");
                // Ok so something very CRITICAL is about to happen
                //  brace yourself
                //  ...
                //  Ok so at this point in time we read a stream packet
                //  head byte, counted 31 bytes, and now our 33 byte
                //  turns out to not be the 0xFX (X:0-15) that we
                //  were expecting! My god! What does this mean? Well
                //  it could mean:
                //      1) that this is not a stream packet and we just
                //          an ASCII 'A' and we were looking for it
                //          a scenario where this could happen is OTA
                //          programming
                //      2) That this WAS a STREAM packet and some how
                //          that packet got fucked up on its way to
                //          RFduino and now we need to chalk this up
                //          as a loss and prepare ourselves for a new
                //          stream packet
                //
                // For now, let's just say, hey look for another stream packet
                //  should we take this oppertunity to see if this byte is A
                if (newChar == OPENBCI_STREAM_PACKET_HEAD) {
                    // Reset the stream packet byte counter
                    streamPacketBuffer.bytesIn = 1;
                    // gotHead is already true
                    // Serial.println("HEAD: 1");
                } else {
                    streamPacketBuffer.gotHead = false;
                }
            }
        }
    } else {
        // is this byte a HEAD? 'A'?
        if (newChar == 'A') {
            // Serial.println("HEAD: 1");
            // Reset the stream packet byte counter
            streamPacketBuffer.bytesIn = 1;

            // Set flag to tell system that we could have a stream packet on our hands
            streamPacketBuffer.gotHead = true;
        } else {
            // Serial.print("head test fail: "); Serial.println(newChar);
        }
    }
}

/**
* @description Check to see if there is data in the bufferRadio. This function
*                 is intended to be called in the loop(). check and see if we
*                 are ready to send data from the bufferRadio. We should only
*                 send data when we are sure we got all the packets from
*                 RFduinoGZLL_onReceive()
* @returns: Returns a TRUE if got all the packets and FALSE if not
* @author AJ Keller (@pushtheworldllc)
*/
// boolean OpenBCI_Radios_Class::isTheDevicesRadioBufferFilledWithAllThePacketsFromTheHost(void) {
//     // Check to see if we got all the packets
//     if (bufferPacketsToReceive == bufferPacketsReceived) {
//         return true;
//     } else {
//         return false;
//     }
// }

/**
* @description Called when readRadio returns true. We always write the contents
*                 of bufferRadio over Serial.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::writeTheDevicesRadioBufferToThePic(void) {
    if (debugMode) {
        for (int j = 0; j < bufferPositionWriteRadio; j++) {
            Serial.print(bufferRadio[j]);
        }
        Serial.println();
    } else {
        for (int j = 0; j < bufferPositionWriteRadio; j++) {
            Serial.write(bufferRadio[j]);
        }
    }
    bufferCleanRadio();
}

/********************************************/
/********************************************/
/*************    PASS THRU    **************/
/********************************************/
/********************************************/

/**
 * @description used to flash the led to indicate to the user the device is in pass through mode.
 */
void OpenBCI_Radios_Class::ledFeedBackForPassThru(void) {
    digitalWrite(OPENBCI_PIN_HOST_LED,HIGH);
    delay(600);
    digitalWrite(OPENBCI_PIN_HOST_LED,LOW);
    delay(200);
}

/********************************************/
/********************************************/
/********    COMMON MEHTOD CODE    **********/
/********************************************/
/********************************************/

/**
* @description Writes a buffer to the serial port of a given length
* @param buffer [char *] The buffer you want to write out
* @param length [int] How many bytes to you want to write out?
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::writeBufferToSerial(char *buffer, int length) {
    // Make sure we don't seg fault
    if (buffer == NULL) return;

    // Loop through all bytes in buffer
    for (int i = 0; i < length; i++) {
        Serial.write(buffer[i]);
    }
}

/**
* @description Private function to clear the given buffer of length
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanChar(char *buffer, int bufferLength) {
    for (int i = 0; i < bufferLength; i++) {
        buffer[i] = 0;
    }
}

/**
* @description Private function to clean a PacketBuffer.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanPacketBuffer(PacketBuffer *packetBuffer, int numberOfPackets) {
    for(int i = 0; i < numberOfPackets; i++) {
        packetBuffer[i].positionRead = 0;
        packetBuffer[i].positionWrite = 1;
    }
}

/**
* @description Private function to clean a PacketBuffer.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanCompletePacketBuffer(PacketBuffer *packetBuffer, int numberOfPackets) {
    for(int i = 0; i < numberOfPackets; i++) {
        packetBuffer[i].positionRead = 0;
        packetBuffer[i].positionWrite = 0;
    }
}

/**
* @description Private function to clean (clear/reset) a Buffer.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanBuffer(Buffer *buffer, int numberOfPacketsToClean) {
    bufferCleanPacketBuffer(buffer->packetBuffer,numberOfPacketsToClean);
    buffer->numberOfPacketsToSend = 0;
    buffer->numberOfPacketsSent = 0;
}

/**
* @description Private function to clean (clear/reset) a Buffer.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanCompleteBuffer(Buffer *buffer, int numberOfPacketsToClean) {
    bufferCleanCompletePacketBuffer(buffer->packetBuffer,numberOfPacketsToClean);
    buffer->numberOfPacketsToSend = 0;
    buffer->numberOfPacketsSent = 0;
}

/**
* @description Private function to clean (clear/reset) the bufferRadio.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferCleanRadio(void) {
    bufferCleanChar(bufferRadio,OPENBCI_BUFFER_LENGTH);
    bufferPacketsReceived = 0;
    bufferPacketsToReceive = 0;
    bufferPositionReadRadio = 0;
    bufferPositionWriteRadio = 0;
    isTheDevicesRadioBufferFilledWithAllThePacketsFromTheHost = false;
    isTheHostsRadioBufferFilledWithAllThePacketsFromTheDevice = false;
}

/**
 * @description Private function to clean (clear/reset) the bufferSerial.
 * @param - `numberOfPacketsToClean` - [int] - The number of packets you want to
 *      clean, for example, on init, we would clean all packets, but on cleaning
 *      from the RFduinoGZLL_onReceive() we would only clean the number of
 *      packets acutally used.
 * @author AJ Keller (@pushtheworldllc)
 */
void OpenBCI_Radios_Class::bufferCleanSerial(int numberOfPacketsToClean) {
    bufferCleanBuffer(&bufferSerial, numberOfPacketsToClean);
    currentPacketBufferSerial = bufferSerial.packetBuffer;
    previousPacketNumber = 0;
}

/**
 * @description Function to clean (clear/reset) the bufferStreamPackets.
 * @param - `numberOfPacketsToClean` - [int] - The number of packets you want to
 *      clean, for example, on init, we would clean all packets, but on cleaning
 *      from the RFduinoGZLL_onReceive() we would only clean the number of
 *      packets acutally used.
 * @author AJ Keller (@pushtheworldllc)
 */
void OpenBCI_Radios_Class::bufferCleanStreamPackets(int numberOfPacketsToClean) {
    bufferCleanCompleteBuffer(&bufferStreamPackets, numberOfPacketsToClean);
    currentPacketBufferStreamPacket = bufferStreamPackets.packetBuffer;
}

/**
 * @description Resets the stream packet buffer to default settings
 */
void OpenBCI_Radios_Class::bufferResetStreamPacketBuffer(void) {
    streamPacketBuffer.gotHead = false;
    streamPacketBuffer.bytesIn = 0;
    streamPacketBuffer.readyForLaunch = false;
}

/**
* @description Moves bytes on serial port into bufferSerial
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferSerialFetch(void) {
    // store the total number of bytes to read, this is faster then calling
    //  Serial.available() every loop
    // int numberOfBytesToRead = Serial.available();

    // Set the number of packets to 1 initally, it will only grow
    if (bufferSerial.numberOfPacketsToSend == 0) {
        bufferSerial.numberOfPacketsToSend = 1;
    }
    //
    // int bytesThisRead = ;

    // We are going to call Serial.read() as many times as there are bytes on the
    //  buffer, on each call we are going to insert into
    // Serial.print("Read "); Serial.print(bytesThisRead); Serial.println(" bytes");
    while (Serial.available()) {

        // If positionWrite >= OPENBCI_BUFFER_LENGTH need to wrap around
        if (currentPacketBufferSerial->positionWrite >= OPENBCI_MAX_PACKET_SIZE_BYTES) {
            // Go to the next packet
            bufferSerial.numberOfPacketsToSend++;
            // Did we run out of buffers?
            if (bufferSerial.numberOfPacketsToSend >= OPENBCI_MAX_NUMBER_OF_BUFFERS) {
                // this is bad, so something, throw error, explode... idk yet...
                //  for now set currentPacketBufferSerial to NULL
                currentPacketBufferSerial = NULL;
                // Clear out buffers... start again!
                bufferCleanSerial(OPENBCI_MAX_NUMBER_OF_BUFFERS);

                // Need an emergency breaking system...
                if (isDevice) {
                    char msg[1];
                    msg[0] = (char)ORPM_DEVICE_SERIAL_OVERFLOW;
                    RFduinoGZLL.sendToHost(msg,1);
                    Serial.print('v'); // issue a software reset to the board
                } else if (isHost){
                    Serial.print("Input too large!$$$");
                }

            } else {
                // move the pointer 1 struct
                currentPacketBufferSerial++;
            }
        }

        // We are only going to mess with the current packet if it's not null
        if (currentPacketBufferSerial) {
            char newChar = Serial.read();
            // Store the byte to current buffer at write postition
            currentPacketBufferSerial->data[currentPacketBufferSerial->positionWrite] = newChar;

            // Increment currentPacketBufferSerial write postion
            currentPacketBufferSerial->positionWrite++;

            // We need to do some checking for a stream packet
            //  this only appliest to the device, so let's ask that question first
            if (isDevice) {
                // follow the write rabbit!
                processCharForStreamPacket(newChar);
            }

        } else {
            if (verbosePrintouts) {
                Serial.print("BO:"); Serial.println(Serial.read());
            } else {
                Serial.read();
            }
        }

        if (isDevice) {
            pollRefresh();
        }

        // Save time of last serial read...
        lastTimeNewSerialDataWasAvailable = millis();
    }
}

/**
* @description Moves bytes into bufferStreamPackets from on_recieve
* @param `data` - {char *} - Normally a buffer to read into bufferStreamPackets
* @param `length` - {int} - Normally 32, but you know, who wants to read what we shouldnt be...
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::bufferAddStreamPacket(char *data, int length) {

    // Set the number of packets to 1 initally, it will only grow
    if (bufferStreamPackets.numberOfPacketsToSend == 0) {
        bufferStreamPackets.numberOfPacketsToSend = 1;
    }

    for (int i = 0; i < length; i++) {
        // If positionWrite >= OPENBCI_BUFFER_LENGTH need to wrap around
        if (currentPacketBufferStreamPacket->positionWrite >= OPENBCI_MAX_PACKET_SIZE_BYTES) {
            // Go to the next packet
            bufferStreamPackets.numberOfPacketsToSend++;
            // Did we run out of buffers?
            if (bufferStreamPackets.numberOfPacketsToSend >= OPENBCI_MAX_NUMBER_OF_BUFFERS) {
                // this is bad, so something, throw error, explode... idk yet...
                //  for now set currentPacketBufferSerial to NULL
                currentPacketBufferStreamPacket = NULL;
            } else {
                // move the pointer 1 struct
                currentPacketBufferStreamPacket++;
            }
        }
        // We are only going to mess with the current packet if it's not null
        if (currentPacketBufferStreamPacket) {
            // Store the byte to current buffer at write postition
            currentPacketBufferStreamPacket->data[currentPacketBufferStreamPacket->positionWrite] = data[i];

            // Increment currentPacketBufferSerial write postion
            currentPacketBufferStreamPacket->positionWrite++;
        }
    }
}

/**
* @description Strips and gets the check sum from a byteId
* @param byteId [char] a byteId (see ::byteIdMake for description of bits)
* @returns [int] the check sum
*/
char OpenBCI_Radios_Class::byteIdGetCheckSum(char byteId) {
    return byteId & 0x07;
}

/**
* @description Determines if this byteId is a stream byte
* @param byteId [char] a byteId (see ::byteIdMake for description of bits)
* @returns [int] the check sum
*/
boolean OpenBCI_Radios_Class::byteIdGetIsStream(char byteId) {
    return byteId > 0x7F;
}

/**
* @description Strips and gets the packet number from a byteId
* @param byteId [char] a byteId (see ::byteIdMake for description of bits)
* @returns [int] the packetNumber
*/
int OpenBCI_Radios_Class::byteIdGetPacketNumber(char byteId) {
    return (int)((byteId & 0x78) >> 3);
}

/**
* @description Strips and gets the packet number from a byteId
* @param byteId [char] a byteId (see ::byteIdMake for description of bits)
* @returns [byte] the packet type
*/
byte OpenBCI_Radios_Class::byteIdGetStreamPacketType(char byteId) {
    return (byte)((byteId & 0x78) >> 3);
}

/**
* @description Creates a byteId for sending data over the RFduinoGZLL
* @param isStreamPacket [boolean] Set true if this is a streaming packet
* @param packetNumber [int] What number packet are you trying to send?
* @param data [char *] The data you want to send. NOTE: Do not send the address
*           of the entire buffer, send this method address of buffer + 1
* @param length [int] The length of the data buffer
* @returns [char] The newly formed byteId where a byteId is defined as
*           Bit 7 - Streaming byte packet
*           Bits[6:3] - Packet count
*           Bits[2:0] - The check sum
* @author AJ Keller (@pushtheworldllc)
*/
char OpenBCI_Radios_Class::byteIdMake(boolean isStreamPacket, int packetNumber, char *data, int length) {
    // Set output initially equal to 0
    char output = 0x00;

    // Set first bit if this is a streaming packet
    if  (isStreamPacket) output = output | 0x80;

    // Set packet count bits Bits[6:3] NOTE: 0xFF is error
    // convert int to char then shift then or
    output = output | ((packetNumber & 0x0F) << 3);

    // Set the check sum bits Bits[2:0]
    output = output | checkSumMake(data,length);

    return output;
}

/**
* @description Strips and gets the packet number from a byteId
* @returns [byte] the packet type
*/
byte OpenBCI_Radios_Class::byteIdMakeStreamPacketType(void) {
    return (byte)(streamPacketBuffer.typeByte & 0x0F);
}

/**
* @description Convience method to compare two chars for equality
* @param data [char *] The data packet from on_recieve
* @param len [int] The length of data packet
* @returns boolean if equal
*/
boolean OpenBCI_Radios_Class::checkSumsAreEqual(char *data, int len) {
    char expectedCheckSum = byteIdGetCheckSum(data[0]);

    char calculatedCheckSum = checkSumMake(data + 1,len - 1);

    return expectedCheckSum == calculatedCheckSum;
}

/**
* @description makes a check sum based off the data of specified length
* @param data [char *] array of bytes to create check sum with
* @param length [int] length of data (you better make sure this is right!)
* @returns [char] of the check sum
* @author AJ Keller (@pushtheworldllc) with Leif Percifield
*/
char OpenBCI_Radios_Class::checkSumMake(char *data, int length) {
    int count;
    unsigned int sum = 0;

    if (data == NULL) return 0x00;

    // initialize count to 0
    count = 0;

    // Use a do-while loop to execute
    do {
        sum = sum + data[count];
        count++;
    } while(--length);

    // Bit smash/smush with unsigned int hack
    sum = -sum;

    return (sum & 0x07);
}

/**
* @description Send a NULL packet to the HOST
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::pollHost(void) {
    RFduinoGZLL.sendToHost(NULL,0);
    pollRefresh();
    // Serial.println(".");
}

/**
* @description Has enough time passed since the last poll
* @return [boolean]
* @author AJ Keller (@pushtheworldllc)
*/
boolean OpenBCI_Radios_Class::pollNow(void) {
    return millis() - timeOfLastPoll > OPENBCI_POLL_TIME_DURATION_MS;
}

/**
* @description Reset the time since last packent sent to HOST
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_Radios_Class::pollRefresh(void) {
    timeOfLastPoll = millis();
}

/**
 * @description Takes a byteId and converts to a Stop Byte for a streaming packet
 * @param `byteId` - [byte] - A byteId with packet type in bits 6-3
 * @return - [byte] - A stop byte with 1100 as the MSBs with packet type in the
 *          four LSBs
 * @example byteId == 0b10111000 returns 0b11000111
 */
byte OpenBCI_Radios_Class::outputGetStopByteFromByteId(char byteId) {
    return byteIdGetStreamPacketType(byteId) | 0xC0;
}

OpenBCI_Radios_Class radio;


/********************************************/
/********************************************/
/*******    RFDUINOGZLL DELEGATE    *********/
/********************************************/
/********************************************/
void RFduinoGZLL_onReceive(device_t device, int rssi, char *data, int len) {
    char msg[1]; // the message to ack back
    // will either send above message or data from buffer serial
    boolean goodToAddPacketToRadioBuffer = true;
    boolean willSendDataFromBufferSerial = false;
    boolean willSendMsg = false;

    if (radio.isHost) {
        radio.lastTimeHostHeardFromDevice = millis();
    }

    if (len == 1) { // this is a radio comm packet
        /**************************************/
        // CAN INITIATE DATA SEND HERE
        /**************************************/
        char privateRadioCode = data[0];
        if (radio.isWaitingForNewChannelNumber) {
            radio.isWaitingForNewChannelNumber = false;
            // Refresh poll
            radio.pollRefresh();
            // Set the new channel number
            boolean success = radio.setChannelNumber((uint32_t)data[0]);
            if (success) {
                // Change Device radio channel
                RFduinoGZLL.channel = (uint32_t)data[0];
                // Poll the host, which should have swapped over now...
                radio.pollHost();
            }
        } else {
            switch (data[0]) {
                case ORPM_PACKET_BAD_CHECK_SUM:
                    // Resend the last sent packet
                    radio.bufferSerial.numberOfPacketsSent--;

                    willSendDataFromBufferSerial = true;
                    if (radio.verbosePrintouts) {
                        Serial.println("R<-B");
                    }
                    break;
                case ORPM_PACKET_MISSED:
                    // Start the page transmission over again
                    radio.bufferSerial.numberOfPacketsSent = 0;

                    willSendDataFromBufferSerial = true;
                    if (radio.verbosePrintouts) {
                        Serial.println("R<-M");
                    }
                    break;
                case ORPM_CHANGE_CHANNEL_HOST_REQUEST:
                    // The host want to change the channel!
                    // We need to tell the Host we acknoledge his request and are
                    // Patiently waiting for the channel he wants to change to
                    if (radio.verbosePrintouts) {
                        Serial.println("R<-CCHR");
                    }
                    if (radio.isHost) {
                        msg[0] = (char)ORPM_INVALID_CODE_RECEIVED;
                        RFduinoGZLL.sendToDevice(device,msg,1);
                    } else {
                        // Tell the Host we are ready to change channels
                        msg[0] = (char)ORPM_CHANGE_CHANNEL_DEVICE_READY;
                        radio.isWaitingForNewChannelNumber = true;
                        RFduinoGZLL.sendToHost(msg,1);
                        radio.pollRefresh();
                    }
                    break;
                case ORPM_CHANGE_CHANNEL_DEVICE_READY:
                    // We are the Host, and the device is ready to change it's channel number to what every we want
                    if (radio.verbosePrintouts) {
                        Serial.println("R<-CCDR");
                    }
                    if (radio.isHost) {
                        // send back the radio channel
                        msg[0] = (char)radio.radioChannel;
                        RFduinoGZLL.sendToDevice(device,msg,1);
                        radio.setChannelNumber(radio.radioChannel);
                        RFduinoGZLL.channel = radio.radioChannel;
                    } else {
                        // Send the channel to change to to the device
                        msg[0] = (char)ORPM_CHANGE_CHANNEL_DEVICE_READY;
                        RFduinoGZLL.sendToHost(msg,1);
                        radio.pollRefresh();
                    }
                    break;
                default:
                    msg[0] = (char)ORPM_INVALID_CODE_RECEIVED;
                    if (radio.isHost) {
                        RFduinoGZLL.sendToDevice(device,msg,1);
                    } else {
                        RFduinoGZLL.sendToHost(msg,1);
                        radio.pollRefresh();
                    }
                    break;
            }
        }
    } else if (len > 1) {
        /**************************************/
        // CAN SEND DATA HERE
        /**************************************/

        // We enter this if statement if we got a packet with length greater than one... it's important to note this is for both the Host and for the Device.

        // A general rule of this system is that if we recieve a packet with a packetNumber of 0 that signifies an actionable end of transmission

        boolean gotLastPacket = false;

        // The packetNumber is embedded in the first byte, the byteId
        int packetNumber = radio.byteIdGetPacketNumber(data[0]);

        // When in debug mode, state which packetNumber we just recieved
        if (radio.verbosePrintouts) {
            Serial.print("R<-");Serial.println(packetNumber);
        }

        // Verify the checksums are equal
        if (radio.checkSumsAreEqual(data,len)) {
            // This first statment asks if this is a last packet and the previous
            //  packet was 0 too, this is in an effort to get to the point in the
            //  program where we ask if this packet is a stream packet
            if (packetNumber == 0 && radio.previousPacketNumber == 0) {
                // This is a one packet message
                gotLastPacket = true;

            } else {
                if (packetNumber > 0 && radio.previousPacketNumber == 0) {
                    // This is the first of multiple packets we are recieving
                    radio.previousPacketNumber = packetNumber;

                } else {
                    // This is not the first packet we are reciving of this page
                    if (radio.previousPacketNumber - packetNumber == 1) { // Normal...
                        // update
                        radio.previousPacketNumber = packetNumber;

                        // Is this the last packet?
                        if (packetNumber == 0) {
                            // Serial.println("Last packet of multiple.");
                            gotLastPacket = true;
                        }

                    } else {
                        goodToAddPacketToRadioBuffer = false;
                        // We missed a packet, send resend message
                        msg[0] = ORPM_PACKET_MISSED & 0xFF;

                        // reset ring buffer to start
                        radio.bufferPositionWriteRadio = 0;

                        // Reset the packet state
                        radio.previousPacketNumber = 0;
                        if (radio.verbosePrintouts) {
                            Serial.println("S->M");
                        }
                    }
                }
            }
        } else {
            goodToAddPacketToRadioBuffer = false;
            msg[0] = ORPM_PACKET_BAD_CHECK_SUM & 0xFF;
            if (radio.verbosePrintouts) {
                Serial.println("S->B");
            }
        }

        // goodToAddPacketToRadioBuffer is true if we have not recieved an error
        //  message and this packet should be routed to either the Pic (if we're
        //  the Device) or the Driver (if we are the Host)
        if (goodToAddPacketToRadioBuffer) {
            // Check to see if this is a stream packet... This the case then the
            //  Device has sent a stream packet to the Host
            if (radio.byteIdGetIsStream(data[0])) {
                // Serial.println("Got stream packet!");
                // We don't actually read to serial port yet, we simply move it
                //  into a buffer in an effort to not write to the Serial port
                //  from an ISR.
                radio.bufferAddStreamPacket(data,len);
            } else {
                // This is not a stream packet and, be default, we will store it
                //  into a buffer called bufferRadio that is a 1 dimensional array
                for (int i = 1; i < len; i++) { // skip the byteId
                    if (radio.bufferPositionWriteRadio < OPENBCI_BUFFER_LENGTH) { // Check for to prevent overflow
                        radio.bufferRadio[radio.bufferPositionWriteRadio] = data[i];
                        radio.bufferPositionWriteRadio++;
                    }
                }
                // If this is the last packet then we need to set a flag to empty
                //  the buffer
                if (gotLastPacket) {
                    // flag contents of radio buffer to be printed!
                    radio.isTheHostsRadioBufferFilledWithAllThePacketsFromTheDevice = true;
                    radio.isTheDevicesRadioBufferFilledWithAllThePacketsFromTheHost = true;
                }
            }
            if (radio.bufferSerial.numberOfPacketsSent < radio.bufferSerial.numberOfPacketsToSend) {
                if (radio.theLastTimeNewSerialDataWasAvailableWasLongEnough()) {
                    willSendDataFromBufferSerial = true;
                }
            } else if (radio.bufferSerial.numberOfPacketsSent == radio.bufferSerial.numberOfPacketsToSend && radio.bufferSerial.numberOfPacketsToSend != 0) {
                // Serial.println("Cleaning Hosts's bufferSerial");
                // Clear buffer
                radio.bufferCleanSerial(radio.bufferSerial.numberOfPacketsSent);
            } else {
                if (radio.isDevice) {
                    radio.pollHost();
                }
            }


            if (radio.verbosePrintouts) {
                Serial.println("S->N");
            }
        } else { // We got a problem

            if (radio.isHost) {
                RFduinoGZLL.sendToDevice(device,msg,1);
            } else {
                RFduinoGZLL.sendToHost(msg,1);
                radio.pollRefresh();
            }
        }
    } else { // This is a NULL byte ack
        /**************************************/
        // CAN SEND DATA HERE *****************/
        /**************************************/

        // More packets to send?
        if (radio.isWaitingForNewChannelNumberConfirmation) {
            radio.isWaitingForNewChannelNumberConfirmation = false;
            Serial.write(OPENBCI_HOST_CHANNEL_CHANGE_SUCCESS);

        } else if (radio.bufferSerial.numberOfPacketsSent < radio.bufferSerial.numberOfPacketsToSend) {
            if (radio.theLastTimeNewSerialDataWasAvailableWasLongEnough()) {
                willSendDataFromBufferSerial = true;
            }

        } else { // We are done sending packets
            // Clean bufferSerial
            if (radio.bufferSerial.numberOfPacketsSent == radio.bufferSerial.numberOfPacketsToSend && radio.bufferSerial.numberOfPacketsToSend != 0) {
                // Serial.println("Cleaning Hosts's bufferSerial");
                // Clear buffer
                radio.bufferCleanSerial(radio.bufferSerial.numberOfPacketsSent);
            }
        }
    }


    if (willSendDataFromBufferSerial) {
        // Build byteId
        // char byteIdMake(boolean isStreamPacket, int packetNumber, char *data, int length)
        int packetNumber = radio.bufferSerial.numberOfPacketsToSend - radio.bufferSerial.numberOfPacketsSent - 1;
        char byteId = radio.byteIdMake(false,packetNumber,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data + 1, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite - 1);

        // Add the byteId to the packet
        (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[0] = byteId;

        // Send back some data!
        if (radio.isHost) {
            // Only parse single packet messages for keys... multi packet can just be sent
            if (radio.bufferSerial.numberOfPacketsToSend == 1 && packetNumber == 0) {
                // Single byte codes
                if ((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite == 2) {
                    if ((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[1] == OPENBCI_HOST_TIME_SYNC) {
                        // ack back to driver to notify packet being sent.
                        Serial.write(OPENBCI_HOST_TIME_SYNC_ACK);
                        RFduinoGZLL.sendToDevice(device,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
                    } else if ((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[1] == OPENBCI_HOST_CHANNEL_QUERY) {
                        Serial.write(radio.getChannelNumber());
                        radio.bufferCleanSerial(1);
                    } else {
                        if (radio.verbosePrintouts) {
                            Serial.print("S->"); Serial.println(packetNumber);
                        }
                        RFduinoGZLL.sendToDevice(device,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
                    }
                // Multi byte codes
                } else if ((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite == 3) {
                    if ((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[1] == OPENBCI_HOST_CHANNEL_CHANGE) {
                        // Need to start the channel change process....
                        if (data[2] > RFDUINOGZLL_CHANNEL_LIMIT_UPPER) {
                            // Tell the driver we failed
                            Serial.write(OPENBCI_HOST_CHANNEL_CHANGE_INVALID);
                        } else {
                            msg[0] = (char)ORPM_CHANGE_CHANNEL_HOST_REQUEST;
                            if (radio.verbosePrintouts) {
                                Serial.print("New channel: "); Serial.println((uint32_t)(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[2]);
                            }
                            // Store the old channel number
                            radio.previousRadioChannel = radio.getChannelNumber();
                            // Store the new channel number
                            radio.radioChannel = (uint32_t)(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data[2];
                            RFduinoGZLL.sendToDevice(device,msg,1);
                        }
                    } else {
                        if (radio.verbosePrintouts) {
                            Serial.print("S->"); Serial.println(packetNumber);
                        }
                        RFduinoGZLL.sendToDevice(device,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
                    }
                } else {
                    if (radio.verbosePrintouts) {
                        Serial.print("S->"); Serial.println(packetNumber);
                    }
                    RFduinoGZLL.sendToDevice(device,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
                }
            } else {
                if (radio.verbosePrintouts) {
                    Serial.print("S->"); Serial.println(packetNumber);
                }
                RFduinoGZLL.sendToDevice(device,(radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
            }
        } else { //isDevice
            if (radio.verbosePrintouts) {
                Serial.print("S->"); Serial.println(packetNumber);
            }
            RFduinoGZLL.sendToHost((radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->data, (radio.bufferSerial.packetBuffer + radio.bufferSerial.numberOfPacketsSent)->positionWrite);
            radio.pollRefresh();
        }

        radio.bufferSerial.numberOfPacketsSent++;
    }

}