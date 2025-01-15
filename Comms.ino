#include "Comms.h"

void requestData(uint16_t timeout)
{
   Serial2.setTimeout(timeout);

  // flush input buffer

   Serial2.write('n');

  // wait for data or timeout
  uint32_t start = millis();
  uint32_t end = start;
  while ( Serial2.available() < 3 && (end - start) < timeout)
  {
    end = millis();
  }

  // if within timeout, read data
  if (end - start < timeout)
  {
    // skip first two bytes
     Serial2.read(); // 'n'
     Serial2.read(); // 0x32
    uint8_t dataLen =  Serial2.read();
     Serial2.readBytes(buffer, dataLen);
  }
}

bool getBit(uint16_t address, uint8_t bit)
{
  return bitRead(buffer[address], bit);
}

uint8_t getByte(uint16_t address)
{
  return buffer[address];
}

uint16_t getWord(uint16_t address)
{
  return makeWord(buffer[address + 1], buffer[address]);
}