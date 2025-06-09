#include <XModem.h>
#include <string.h>
#include <LibPrintf.h>

int OE = 19;
int WE = 29;
int CE = 15;

const unsigned int dataPin[] = {32, 33, 34, 5, 7, 9, 11, 13};
const unsigned int addressPin[] = {35, 36, 37, 38, 39, 40, 41, 42, 25, 23, 17, 21, 43, 27, 44, 31};

// ..\SerialComm\bin\Debug\netcoreapp3.1\SerialComm.exe com6 115200
XModem xmodem;
byte chunkData[64];
int addressSize = 15;
uint16_t chunkSize = 64;
uint16_t currentAddress = 0;
const unsigned int WCT = 8; // 7;
int maxAddress = 0;
const boolean logData = false;
int size = 0;
char buf[100];
String command;

void setup()
{
  Serial.begin(115200);
  maxAddress = (int)pow(2, addressSize);
  size = sizeof(addressPin) / sizeof(addressPin[0]);
  initialState();
}

void initialState()
{

  for (unsigned int pin = 0; pin < addressSize; pin += 1)
  {
    pinMode(addressPin[pin], INPUT);
  }

  for (unsigned int pin = 0; pin < 8; pin += 1)
  {
    pinMode(dataPin[pin], INPUT);
  }

  pinMode(CE, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(WE, OUTPUT);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);

  Serial.println("OK");
}

// Read a line of data from the serial connection.
char *readLine(char *buffer, int len)
{
  for (int ix = 0; (ix < len); ix++)
  {
    buffer[ix] = 0;
  }

  // read serial data until linebreak or buffer is full
  char c = ' ';
  int ix = 0;
  do
  {
    if (Serial.available())
    {
      c = Serial.read();
      if ((c == '\b') && (ix > 0))
      {
        // Backspace, forget last character
        --ix;
      }
      buffer[ix++] = c;
      Serial.write(c);
    }
  } while ((c != '\n') && (c != '\r') && (ix < len));

  buffer[ix - 1] = 0;
  return buffer;
}

char line[120];

String convertToString(char *a, int size)
{
  int i;
  String s = "";
  for (i = 0; i < size; i++)
  {
    s = s + a[i];
  }
  return s;
}

void loop()
{

  Serial.flush();
  readLine(line, sizeof(line));
  command = convertToString(line, sizeof(line));
  command.trim();
  command.toLowerCase();
  if (command.startsWith("c "))
  {
    commandClear(command);
  }
  else if (command.equals("l"))
  {
    commandRead("r");
    currentAddress = currentAddress + 256;
  }
  else if (command.startsWith("r "))
  {
    commandRead(command);
  }
  else if (command.startsWith("w "))
  {
    commandWrite(command);
  }
  else if (command.startsWith("u"))
  {
    commandUpload(command);
  }
  else
  {
    commandGo("g " + command);
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

void commandClear(String command)
{
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
      commandWritePage(startAddress, chunkData, endAddress - startAddress, true);
      Serial.println();
      Serial.println("OK");
      break;
    }

    command = command.substring(index + 1);
    parmNo = parmNo + 1;
  }
}

void printData(uint16_t startAddress, uint16_t endAddress)
{
  unsigned int itemCount = 1;
  String chars = "";
  for (unsigned int i = startAddress; i <= endAddress; i++)
  {
    if (itemCount == 1)
      printf("\n%04x: ", i);
    else if (itemCount == 9)
      printf(" ");
    byte value = readEEPROM(i);
    printf("%02x ", value);
    
    chars = chars + String(value >= 32 && value < 128 ? static_cast<char>(value) : '.');
    
    if (itemCount == 16)
    {
      Serial.print(" ");
      Serial.print(chars);
      itemCount = 0;
      chars = "";
    }
    itemCount++;
  }
}

void commandRead(String strCommand)
{
  unsigned int parmNo = 0;
  unsigned int startAddress = 0;
  unsigned int endAddress = 0;

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
        printData(startAddress, endAddress);
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

uint8_t readDataBus()
{
  uint8_t data = 0;
  for (int pin = 7; pin >= 0; pin -= 1)
  {
    data = (data << 1) + digitalRead(dataPin[pin]);
  }
  return data;
}

bool waitForWriteCycleEnd(byte lastValue)
{
  // Verify programming complete by reading the last value back until it matches the
  // value written twice in a row.  The D7 bit will read the inverse of last written
  // data and the D6 bit will toggle on each read while in programming mode.
  //
  // This loop code takes about 18uSec to execute.  The max readcount is set to the
  // device's maxReadTime (in uSecs) divided by ten rather than eighteen to ensure
  // that it runs at least as long as the chip's timeout value, even if some code
  // optimizations are made later. In actual practice, the loop will terminate much
  // earlier because it will detect the end of the write well before the max time.
  int mMaxWriteTime = 10;
  byte b1 = 0, b2 = 0;
  setDataPins(INPUT);
  delayMicroseconds(1);
  for (unsigned readCount = 1; (readCount < (mMaxWriteTime * 100)); readCount++)
  {
    digitalWrite(CE, LOW);
    digitalWrite(OE, LOW);
    delayMicroseconds(1);
    b1 = readDataBus();
    digitalWrite(OE, HIGH);
    digitalWrite(CE, HIGH);
    digitalWrite(CE, LOW);
    digitalWrite(OE, LOW);
    delayMicroseconds(1);
    b2 = readDataBus();
    digitalWrite(OE, HIGH);
    digitalWrite(CE, HIGH);
    if ((b1 == b2) && (b1 == lastValue))
      return true;
  }
  return false;
}

void setAddress(uint16_t address, int startBit, int endBit)
{
  for (int j = 0; j < endBit; j++)
  {
    if (j >= startBit)
      digitalWrite(addressPin[j], (address & 1) == 1 ? HIGH : LOW);
    address = address >> 1;
  }
}

void setData(byte data)
{
  for (int pin = 0; pin <= 7; pin += 1)
  {
    digitalWrite(dataPin[pin], data & 1);
    data = data >> 1;
  }
}

void writeChunk(byte chunckData[], uint16_t address, uint16_t chunkSize)
{
  setAddressPin(OUTPUT);
  setDataPins(OUTPUT);
  setAddress(address, 0, addressSize);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  bool status = false;
  for (int i = 0; i < chunkSize; i++)
  {
    // Set Address for chunck
    setAddress(address + i, 0, 6);
    // Set Data
    setData(chunckData[i]);
    delayMicroseconds(1);
    digitalWrite(WE, LOW);
    delayMicroseconds(1);
    digitalWrite(WE, HIGH);
  }
  status = waitForWriteCycleEnd(chunckData[chunkSize - 1]);
  digitalWrite(CE, HIGH);
}

void getDataFromProgmemOrValue(byte *rom, uint16_t position, uint16_t size, bool fixed, uint8_t value)
{
  for (uint16_t i = 0; i < size; i++)
  {
    chunkData[i] = fixed ? value : pgm_read_word_near(&(rom[position + i]));
    // printf("returnData[%04X] = %02X\n", i, returnData[position+i]);
  }
}

void commandWritePage(uint16_t address, byte *rom, uint16_t size, bool clear)
{

  printf("Start recording...\n\n");
  unsigned long startTime = millis();
  uint16_t chunkQty = size / chunkSize;
  // print orientation
  printf("[");
  for (uint16_t q = 0; q < chunkQty; q++)
    printf("-");
  Serial.println("]");
  Serial.print(" ");

  for (uint16_t c = 0; c < chunkQty; c++)
  {
    getDataFromProgmemOrValue(rom, c * chunkSize, chunkSize, clear, 0xff);
    writeChunk(chunkData, address + c * chunkSize, chunkSize);
    printf("#");
  }
  unsigned long endTime = millis();
  unsigned long elapsedTime = endTime - startTime;
  printf("\n\ntime: %5.2fs\n", float(elapsedTime) / 1000);
}

bool process_block(void *blk_id, size_t idSize, byte *data, size_t dataSize)
{
  int chunkId = 0;
  int chunk = 0;
  for (int i = 0; i < dataSize; i++)
  {
    if (chunkId == 63)
    {
      writeChunk(chunkData, currentAddress, 64);
      currentAddress = currentAddress + 64;
      chunkId = 0;
      chunk++;
    }
    chunkData[chunkId] = data[i];
    chunkId++;
  }

  writeChunk(chunkData, currentAddress, chunkId);

  // return false to stop the transfer early
  return true;
}

void Upload(uint16_t address)
{
  currentAddress = address;
  printf("Waiting for upload... in %04X\n\n", currentAddress);
  unsigned long startTime = millis();
  xmodem.begin(Serial, XModem::ProtocolType::XMODEM);
  xmodem.setRecieveBlockHandler(process_block);

  while (!xmodem.receive())
  {
  }

  printf("Received.\n\n");
  unsigned long endTime = millis();
  unsigned long elapsedTime = endTime - startTime;
  printf("\n\ntime: %5.2fs\n", float(elapsedTime) / 1000);
}

void commandUpload(String strCommand)
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
      Upload(address);
      break;
    }

    strCommand = strCommand.substring(index + 1);
    parmNo = parmNo + 1;
  }
  currentAddress = address;
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
      // Serial.println();Serial.print("parno: "); Serial.print(parmNo);Serial.print("strCommand: "); Serial.print(strCommand); Serial.println();
      if (itemCount == 0)
      {
        // Serial.println();
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

int StrToDec(String parm)
{
  int str_len = parm.length() + 1;
  char char_array[str_len];
  parm.toCharArray(char_array, str_len);
  return (int)strtol(char_array, 0, 16);
}

void setRead()
{
  digitalWrite(CE, LOW);
  digitalWrite(OE, LOW);
  for (unsigned int pin = 0; pin < 8; pin += 1)
  {
    pinMode(dataPin[pin], INPUT);
  }
}

void setWrite()
{
  digitalWrite(CE, LOW);
  digitalWrite(OE, HIGH);
  for (unsigned int pin = 0; pin < 8; pin += 1)
  {
    pinMode(dataPin[pin], OUTPUT);
  }
  delay(WCT);
}

void setDataPins(uint8_t IN_OUT)
{
  for (unsigned int pin = 0; pin < 8; pin += 1)
  {
    pinMode(dataPin[pin], IN_OUT);
  }
  delay(WCT);
}

void setStandby()
{
  for (unsigned int pin = 0; pin < addressSize; pin += 1)
  {
    pinMode(addressPin[pin], INPUT);
  }
  digitalWrite(CE, HIGH);
  digitalWrite(OE, LOW);
}

void setAddressPin(uint8_t IN_OUT)
{
  for (unsigned int pin = 0; pin < addressSize; pin += 1)
  {
    pinMode(addressPin[pin], IN_OUT);
  }
}

void setAddress(int address)
{
  if (logData)
    Serial.println("address: " + toBinary(address, 8));

  for (unsigned int pin = 0; pin < addressSize; pin += 1)
  {
    pinMode(addressPin[pin], OUTPUT);
    if (logData)
      Serial.println("pinMode(" + (String)(addressPin[pin]) + ",OUTPUT)");
  }

  String binary;
  for (int i = 0; i < addressSize; i++)
  {
    digitalWrite(addressPin[i], (address & 1) == 1 ? HIGH : LOW);
    if (logData)
      Serial.println("digitalWrite(" + (String)(addressPin[i]) + "," + ((address & 1) == 1 ? "HIGH" : "LOW") + ")");
    address = address >> 1;
  }
}

uint8_t readEEPROM(int address)
{
  setAddress(address);
  uint8_t data = 0;
  for (int pin = 7; pin >= 0; pin -= 1)
  {
    data = (data << 1) + digitalRead(dataPin[pin]);
  }
  if (logData)
    Serial.println("read: " + (String)(data) + ", " + toBinary(data, 8));
  return data;
}

void writeEEPROM(unsigned int address, uint8_t data)
{
  setAddress(address);
  if (logData)
    Serial.println("write: " + (String)(data) + ", " + toBinary(data, 8));
  for (int pin = 0; pin <= 7; pin += 1)
  {
    digitalWrite(dataPin[pin], data & 1);
    if (logData)
      Serial.println("digitalWrite(" + (String)(dataPin[pin]) + "," + ((data & 1) == 1 ? "HIGH" : "LOW") + ")");
    data = data >> 1;
  }
  digitalWrite(WE, LOW);
  digitalWrite(WE, HIGH);
  delay(WCT);
}

String toBinary(int n, int len)
{
  String binary;

  for (unsigned i = (1 << len - 1); i > 0; i = i / 2)
  {
    binary += (n & i) ? "1" : "0";
  }

  return binary;
}

void writeByte(unsigned int startAddress, byte data)
{
  setWrite();
  writeEEPROM(startAddress, data);
  setStandby();
}
