// Developer: 
//        Adam Varga, 2020, All rights reserved.
// Licence: 
//        Licenced under the MIT licence. See LICENCE file in the project root.
// Usage of this code: 
//        This code creates the interface between the car
//        and the canSniffer_GUI application. If the RANDOM_CAN
//        define is set to 1, this code is generating random
//        CAN packets in order to test the higher level code.
//        The received packets will be echoed back. If the 
//        RANDOM_CAN define is set to 0, the CAN_SPEED define 
//        has to match the speed of the desired CAN channel in
//        order to receive and transfer from and to the CAN bus.
//        Serial speed is 250000baud <- might need to be increased.
// Required arduino packages: 
//        - CAN by Sandeep Mistry (https://github.com/sandeepmistry/arduino-CAN)
// Required modifications: 
//        - MCP2515.h: 16e6 clock frequency reduced to 8e6 (depending on MCP2515 clock)
//        - MCP2515.cpp: extend CNF_MAPPER with your desired CAN speeds
//------------------------------------------------------------------------------
#include <CAN.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Inits, globals
typedef struct {
  long id;
  byte rtr;
  byte ide;
  byte dlc;
  byte dataArray[20];
} packet_t;

int mode = 0;
//int can_speed = 0;

const char SEPARATOR = ',';
const char TERMINATOR = '\n';
const char RXBUF_LEN = 100;
//------------------------------------------------------------------------------
// Printing a packet to serial
void printHex(long num) {
  if ( num < 0x10 ){ Serial.print("0"); }
  Serial.print(num, HEX);
}

void printPacket(packet_t * packet) {
  // packet format (hex string): [ID],[RTR],[IDE],[DATABYTES 0..8B]\n
  // example: 014A,00,00,1A002B003C004D\n
  printHex(packet->id);
  Serial.print(SEPARATOR);
  printHex(packet->rtr);
  Serial.print(SEPARATOR);
  printHex(packet->ide);
  Serial.print(SEPARATOR);
  // DLC is determinded by number of data bytes, format: [00]
  for (int i = 0; i < packet->dlc; i++) {
    printHex(packet->dataArray[i]);
  }
  Serial.print(TERMINATOR);
}
//------------------------------------------------------------------------------
// CAN packet simulator
void CANsimulate(void) {
  packet_t txPacket;

  long sampleIdList[] = {0x110, 0x18DAF111, 0x23A, 0x257, 0x412F1A1, 0x601, 0x18EA0C11};
  int idIndex = random (sizeof(sampleIdList) / sizeof(sampleIdList[0]));
  int sampleData[] = {0xA, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F, 0xA0, 0xB1};

  txPacket.id = sampleIdList[idIndex];
  txPacket.ide = txPacket.id > 0x7FF ? 1 : 0;
  txPacket.rtr = 0; //random(2);
  txPacket.dlc = random(1, 9);

  for (int i = 0; i < txPacket.dlc ; i++) {
    int changeByte = random(4);
    if (changeByte == 0) {
      sampleData[i] = random(256);
    }
    txPacket.dataArray[i] = sampleData[i];
  }

  printPacket(&txPacket);
}
//------------------------------------------------------------------------------
// CAN RX, TX
void onCANReceive(int packetSize) {
  // received a CAN packet
  packet_t rxPacket;
  rxPacket.id = CAN.packetId();
  rxPacket.rtr = CAN.packetRtr() ? 1 : 0;
  rxPacket.ide = CAN.packetExtended() ? 1 : 0;
  rxPacket.dlc = CAN.packetDlc();
  byte i = 0;
  while (CAN.available()) {
    rxPacket.dataArray[i++] = CAN.read();
    if (i >= (sizeof(rxPacket.dataArray) / (sizeof(rxPacket.dataArray[0])))) {
      break;
    }
  }
  printPacket(&rxPacket);
}

void sendPacketToCan(packet_t * packet) {
  for (int retries = 10; retries > 0; retries--) {
    bool rtr = packet->rtr ? true : false;
    if (packet->ide){
      CAN.beginExtendedPacket(packet->id, packet->dlc, rtr);
    } else {
      CAN.beginPacket(packet->id, packet->dlc, rtr);
    }
    CAN.write(packet->dataArray, packet->dlc);
    if (CAN.endPacket()) {
      // success
      break;
    } else if (retries <= 1) {
      return;
    }
  }
}

void setCAN(long can_speed) {
  CAN.end();
  delay(10);
  if (!CAN.begin(can_speed)) {
    Serial.println("Starting CAN failed!");
//    while (1);
  }
  // register the receive callback
  CAN.onReceive(onCANReceive);
  Serial.println("CAN RX TX Started");
  
}
//------------------------------------------------------------------------------
// Serial parser
char getNum(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return 0;
}

char * strToHex(char * str, byte * hexArray, byte * len) {
  byte *ptr = hexArray;
  char * idx;
  for (idx = str ; *idx != SEPARATOR && *idx != TERMINATOR; ++idx, ++ptr ) {
    *ptr = (getNum( *idx++ ) << 4) + getNum( *idx );
  }
  *len = ptr - hexArray;
  return idx;
}

void rxParse(char * buf, int len) {
  packet_t rxPacket;
  char * ptr = buf;
  // All elements have to have leading zero!

  // ID
  byte idTempArray[8], tempLen;
  ptr = strToHex(ptr, idTempArray, &tempLen);
  rxPacket.id = 0;
  for (int i = 0; i < tempLen; i++) {
    rxPacket.id |= (long)idTempArray[i] << ((tempLen - i - 1) * 8);
  }

  // RTR
  ptr = strToHex(ptr + 1, &rxPacket.rtr, &tempLen);

  // IDE
  ptr = strToHex(ptr + 1, &rxPacket.ide, &tempLen);

  // DATA
  ptr = strToHex(ptr + 1, rxPacket.dataArray, &rxPacket.dlc);

  if (mode >= 4) {
    // echo back
    printPacket(&rxPacket);
  } else {
    sendPacketToCan(&rxPacket);
  }
}

void RXcallback(void) {
  static int rxPtr = 0;
  static char rxBuf[RXBUF_LEN];
  static long can_speed = 0;

  while (Serial.available() > 0) {
    if (rxPtr >= RXBUF_LEN) {
      rxPtr = 0;
    }
    char c = Serial.read();
    rxBuf[rxPtr++] = c;
    if (c == TERMINATOR) {
      switch(rxBuf[0]) {
        case 'C':
          mode = rxBuf[1] + 1;
          Serial.print("Start sniffing: ");
          Serial.println(mode);
          switch(mode) {
            case 1:
              can_speed = 33e3;
              break;
            case 2:
              can_speed = 95e3;
              break;
            case 3:
              can_speed = 500e3;
              break;
            case 4:
              randomSeed(12345);
              Serial.println("randomCAN started.");
              can_speed = 0;
              break;
            default:
              break;
          }
          setCAN(can_speed);
          break;
        case 'D':
          Serial.println("Stop sniffing.");
          mode = 0;
          break;
        default:
          Serial.println("Send packet.");
          rxParse(rxBuf+1, rxPtr-1);
          break;
      }
      rxPtr = 0;
    }
  }
}

//------------------------------------------------------------------------------
// Setup
void setup() {
  Serial.begin(250000);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#if 0
#endif
}
//------------------------------------------------------------------------------
// Main
void loop() {
  RXcallback();
  if (mode == 4) {
    CANsimulate();
    delay(100);
  }
}
