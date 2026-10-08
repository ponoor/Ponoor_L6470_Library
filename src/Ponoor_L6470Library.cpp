#include <SPI.h>
#include "Ponoor_L6470Library.h"

int AutoDriver::_numBoards;

uint32_t AutoDriver::_irqSave()
{
#if defined(ARDUINO_ARCH_SAMD)
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
#else
  return 0;
#endif
}

void AutoDriver::_irqRestore(uint32_t primask)
{
#if defined(ARDUINO_ARCH_SAMD)
  if (!primask) __enable_irq();
#else
  (void)primask;
#endif
}

// Constructors
AutoDriver::AutoDriver(int position, int CSPin, int resetPin, int busyPin)
{
  _CSPin = CSPin;
  _position = position;
  _resetPin = resetPin;
  _busyPin = busyPin;
  _numBoards++;
  _SPI = &SPI;
}

AutoDriver::AutoDriver(int position, int CSPin, int resetPin)
{
  _CSPin = CSPin;
  _position = position;
  _resetPin = resetPin;
  _busyPin = -1;
  _numBoards++;
  _SPI = &SPI;
}

void AutoDriver::SPIPortConnect(SPIClass *SPIPort)
{
  _SPI = SPIPort;
}

int AutoDriver::busyCheck(void)
{
  if (_busyPin == -1)
  {
    if (getParam(REG_STATUS) & 0x0002) return 0;
    else                           return 1;
  }
  else 
  {
    if (digitalRead(_busyPin) == HIGH) return 0;
    else                               return 1;
  }
}
