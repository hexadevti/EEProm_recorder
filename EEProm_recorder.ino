
#include <string.h>

int OE = 19;
int WE = 29; 
int CE = 15;

const unsigned int dataPin[] = { 32, 33, 34, 5, 7, 9, 11, 13 };
const unsigned int addressPin[] = { 35, 36, 37, 38, 39, 40, 41, 42, 25, 23, 17, 21, 43, 27, 44, 31 };

// Hello World 3e 00 ee 80 d3 ff 18 fa
// Vai e vem 3e 80 d3 00 0f fe 01 28 02 18 f7 d3 00 07 fe 80 28 f0 18 f7
// ..\SerialComm\bin\Debug\netcoreapp3.1\SerialComm.exe com6 115200


int addressSize = 15;
int16_t currentAddress = 0;
const unsigned int WCT = 7;
int maxAddress = 0;
const boolean logData = false;
int size = 0;
char buf[100];
String command;

void setup() {
  Serial.begin(115200);
  maxAddress = (int)pow(2, addressSize);
  size = sizeof(addressPin)/sizeof(addressPin[0]);
  initialState();
}

void initialState() {
  
  for (unsigned int pin = 0; pin < addressSize; pin+=1) {
    pinMode(addressPin[pin], INPUT);
  }

  for (unsigned int pin = 0; pin < 8; pin+=1) {
    pinMode(dataPin[pin], INPUT);
  }

  pinMode(CE, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(WE, OUTPUT);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);

  Serial.println("OK");
}

void loop() {

  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    if (command.startsWith("c ")) {
      commandClear(command);
    }
    else if (command.equals("l")) {
      commandRead("r");
      currentAddress = currentAddress+256;
    } 
    else if (command.startsWith("r ")) {
      commandRead(command);
    } 
    else if (command.startsWith("w ")) {
      commandWrite(command);
    }
    else 
    {
      commandGo("g " + command);
    }
  }
}


void commandGo(String strCommand)
{
  unsigned int parmNo = 0;
  while (strCommand.length() > 0)
  {
    int index = strCommand.indexOf(' ');
    String parm = strCommand.substring(0, index);
    if (parmNo == 1)
    {
      currentAddress = StrToDec(parm);
      Serial.println();
      Serial.println("OK");
      break;
    }
    strCommand = strCommand.substring(index + 1);
    parmNo = parmNo + 1;
  }
}

void commandClear(String command) {
  unsigned int parmNo = 0;
  unsigned int startAddress = 0;
  unsigned int endAddress = 0;
  int dat = 0;
   
  while (command.length() > 0)
  {
    int index = command.indexOf(' ');
    String parm = command.substring(0, index);
    if (parmNo == 1)
    {
      startAddress = StrToDec(parm);
    }
    if (parmNo == 2)
    {
      endAddress = StrToDec(parm);
    }
    if (parmNo == 3)
    {
      dat = StrToDec(parm);
      erase(startAddress, endAddress, dat);
      Serial.println();
      Serial.println("OK");
      break;
    }

    command = command.substring(index + 1);
    parmNo = parmNo + 1;
  }
}

void commandRead(String strCommand)
{
  unsigned int parmNo = 0;
  unsigned int startAddress = 0;
  unsigned int endAddress = 0;
  unsigned int itemCount = 1;
  char buf[60];

  while (strCommand.length() > 0)
  {
    int index = strCommand.indexOf(' ');
    String parm = strCommand.substring(0, index);

    if (strCommand.equals("r"))
    {
      setRead();
      startAddress = currentAddress;
      endAddress = currentAddress + 255;
    }

    if (parmNo == 1)
    {
      setRead();
      startAddress = StrToDec(parm);
    }
    else if (parmNo == 2)
    {
      endAddress = StrToDec(parm);
    }
    else
    {
      if (index == -1)
      {
        for (unsigned int i = startAddress; i <= endAddress; i++)
        {
          if (itemCount == 1)
          {
            Serial.println();
            sprintf(buf, "%04x: ", i);
            Serial.print(buf);
          }
          else if (itemCount == 9)
          {
            Serial.print("    ");
          }
          sprintf(buf, "%02x ", readEEPROM(i));
          Serial.print(buf);
          if (itemCount == 16)
          {
            itemCount = 0;
          }
          itemCount++;
        }
        setStandby();
        Serial.println();
        Serial.println("OK");
        break;
      }
    }
    strCommand = strCommand.substring(index + 1);
    parmNo = parmNo + 1;
  }
}

void commandWrite(String strCommand)
{
  unsigned int parmNo = 0;
  unsigned int address = 0;
  unsigned int itemCount = 0;
  char buf[60];

  while (strCommand.length() > 0)
  {
    int index = strCommand.indexOf(' ');
    String parm = strCommand.substring(0, index);


    if (parmNo == 1)
    {
      address = StrToDec(parm);
    }
    else if (parmNo > 1)
    {
      //Serial.println();Serial.print("parno: "); Serial.print(parmNo);Serial.print("strCommand: "); Serial.print(strCommand); Serial.println();
      if (itemCount == 0)
      {
        //Serial.println();
        sprintf(buf, "%04x: ", address + (parmNo - 2));
        Serial.print(buf);
      }
      int data = StrToDec(parm);
      writeByte(address + (parmNo - 2), data);
      sprintf(buf, "%02x ", data);
      Serial.print(buf);

      if (itemCount == 7)
      {
        Serial.print("    ");
      }
      if (itemCount == 15)
      {
        Serial.println();
        itemCount = 0;
      }
      else 
      {
        itemCount++;
      }

      if (index == -1)
      {
        Serial.println();
        break;
      }
    }
    strCommand = strCommand.substring(index + 1);
    parmNo = parmNo + 1;
  }
  currentAddress = address;
}

int StrToDec(String parm) {
  int str_len = parm.length() + 1; 
  char char_array[str_len];
  parm.toCharArray(char_array, str_len);
  return (int) strtol(char_array, 0, 16);
}

void setRead() {
  digitalWrite(CE, LOW);  
  digitalWrite(OE, LOW);
  for (unsigned int pin = 0; pin < 8; pin+=1) {
    pinMode(dataPin[pin], INPUT);
  }
}

void setWrite() {
  digitalWrite(CE, LOW);  
  digitalWrite(OE, HIGH);
  for (unsigned int pin = 0; pin < 8; pin+=1) {
    pinMode(dataPin[pin], OUTPUT);
  }
  delay(WCT);
}

void setStandby() {
  for (unsigned int pin = 0; pin < addressSize; pin+=1) {
    pinMode(addressPin[pin], INPUT);
  }
  digitalWrite(CE, HIGH); 
  digitalWrite(OE, LOW);
}

void setAddress(int address) {
  if (logData) Serial.println("address: " + toBinary(address, 8));

  for (unsigned int pin = 0; pin < addressSize; pin+=1) {
    pinMode(addressPin[pin], OUTPUT);
    if (logData) Serial.println("pinMode(" + (String)(addressPin[pin]) + ",OUTPUT)");
  }
  
  String binary;
  for (int i = 0; i < addressSize; i++) {
    digitalWrite(addressPin[i], (address & 1) == 1 ? HIGH : LOW);
    if (logData) Serial.println("digitalWrite(" + (String)(addressPin[i]) + "," + ((address & 1) == 1 ? "HIGH" : "LOW") + ")");
    address = address >> 1;
  }
}


uint8_t readEEPROM(int address) {
  setAddress(address);
  uint8_t data = 0;
  for (int pin = 7; pin >= 0; pin -= 1) {
    data = (data << 1) + digitalRead(dataPin[pin]);
  }
  if (logData) Serial.println("read: " + (String)(data) + ", " + toBinary(data, 8));
  return data;
}

void writeEEPROM(unsigned int address, uint8_t data) {
  setAddress(address);
  if (logData) Serial.println("write: " + (String)(data) + ", " + toBinary(data,8));
  for (int pin = 0; pin <= 7; pin += 1) {
    digitalWrite(dataPin[pin], data & 1);
    if (logData) Serial.println("digitalWrite(" + (String)(dataPin[pin]) + "," + ((data & 1) == 1 ? "HIGH" : "LOW" ) + ")");
    data = data >> 1;
  }
  digitalWrite(WE, LOW);
  digitalWrite(WE, HIGH);
  delay(WCT);
}

String toBinary(int n, int len)
{
    String binary;

    for (unsigned i = (1 << len - 1); i > 0; i = i / 2) {
        binary += (n & i) ? "1" : "0";
    }
 
    return binary;
}

void printContents(unsigned int startAddress, unsigned int endAddress ) {
  Serial.println("Reading EEPROM Max addresses: " + (String)((int)pow(2, addressSize)));
  Serial.println();
  setRead();

  for (unsigned int base = startAddress; base < endAddress; base += 16) {
    uint16_t data[16];
    for (int offset = 0; offset < addressSize; offset += 1) {
      data[offset] = readEEPROM(base + offset);
    }
    char buf[60];
    sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x     %02x %02x %02x %02x %02x %02x %02x %02x",
    base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], 
    data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.println(buf);
  }

  setStandby();
}

void erase(unsigned int startAddress, unsigned int endAddress, uint8_t data) {
  setWrite();
  for (unsigned int address = startAddress; address <= endAddress; address += 1) {
    writeEEPROM(address, data);
    if ((address+1) % 128 == 0) {
      Serial.println((String)((float)(address+1) / endAddress * 100) + "%");
    }
  }
  setStandby();
}

void writeByte(unsigned int startAddress, byte data) {
  setWrite();
  writeEEPROM(startAddress, data);
  setStandby();
}

uint8_t readByte(unsigned int address) {
  setRead();
  return readEEPROM(address);
  setStandby();
}

void write(unsigned int startAddress, byte data[]) {
  Serial.println("Writing EEPROM");
  setWrite();

  for (unsigned int a = 0; a < size; a = a + 1)
  {
    if (a % 128 == 0) {
      Serial.println((String)((float)(startAddress + a) / (startAddress + size) * 100) + "%");
    }
    writeEEPROM(startAddress + a, data[a]);
  } 
  Serial.println(" done");
  setStandby();
}

uint16_t readAddress() {
  uint16_t address = 0;
  for (int pin = 15; pin >= 0; pin -= 1) {
    address = (address << 1) + digitalRead(addressPin[pin]);
  }
  if (logData) Serial.println("readAddress: " + (String)(address) + ", " + toBinary(address, 16));
  return address;
}

uint16_t readAddressBits(uint8_t bits) {
  uint16_t address = 0;
  for (int pin = bits; pin > 0; pin -= 1) {
    address = (address << 1) + digitalRead(addressPin[pin]);
  }
  return address;
}

uint8_t readData() {
  uint8_t data = 0;
  // for (unsigned int pin = 0; pin <= 7; pin+=1) {
  //   pinMode(dataPin[pin], INPUT);
  //   if (logData) Serial.println("pinMode(" + (String)(dataPin[pin]) + ",INPUT)");
  // }
  for (int pin = 7; pin >= 0; pin -= 1) {
    data = (data << 1) + digitalRead(dataPin[pin]);
  }
  //if (logData) Serial.println("readData: " + (String)(data) + ", " + toBinary(data, 8));
  return data;
}

uint8_t writeData(uint8_t data) {
  uint8_t retData = data;
  
  for (int pin = 0; pin <= 7; pin += 1) {
    digitalWrite(dataPin[pin], data & 1);
    if (logData) Serial.println("digitalWrite(" + (String)(dataPin[pin]) + "," + ((data & 1) == 1 ? "HIGH" : "LOW" ) + ")");
    data = data >> 1;
  }
  
  return retData;
}

