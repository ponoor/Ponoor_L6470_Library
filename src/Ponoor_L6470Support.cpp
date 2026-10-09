#include "Ponoor_L6470Library.h"
#include <SPI.h>

// AutoDriverSupport.cpp - Contains utility functions for converting real-world 
//  units (eg, steps/s) to values usable by the dsPIN controller. These are all
//  private members of class AutoDriver.

// The value in the ACC register is [(steps/s/s)*(tick^2)]/(2^-40) where tick is
//  250ns (datasheet value)- 0x08A on boot.
// Multiply desired steps/s/s by 0.068719477 to get an appropriate value for this register.
// This is a 12-bit value, but 0xFFF is reserved (datasheet 9.1.5), so the valid
//  range is 0x001 to 0xFFE (14.55 to 59590 steps/s/s). Out-of-range inputs are clamped.
unsigned long AutoDriver::accCalc(float stepsPerSecPerSec)
{
  float temp = stepsPerSecPerSec * 0.068719477F;
  if (temp >= 4094.5F) return 0x00000FFE;
  // Round to nearest so that accCalc(accParse(n)) == n.
  unsigned long value = (temp > 0.0F) ? (unsigned long) (temp + 0.5F) : 0;
  if (value < 1) return 1;
  return value;
}

// One LSB of ACC is 2^-40 / tick^2 = 14.551915 steps/s/s.
float AutoDriver::accParse(unsigned long stepsPerSecPerSec)
{
  return (float)(stepsPerSecPerSec & 0x00000FFF) * 14.551915F;
}

// The calculation for DEC is the same as for ACC. Value is 0x08A on boot.
unsigned long AutoDriver::decCalc(float stepsPerSecPerSec)
{
  float temp = stepsPerSecPerSec * 0.068719477F;
  if (temp >= 4094.5F) return 0x00000FFE;
  // Round to nearest so that accCalc(accParse(n)) == n.
  unsigned long value = (temp > 0.0F) ? (unsigned long) (temp + 0.5F) : 0;
  if (value < 1) return 1;
  return value;
}

float AutoDriver::decParse(unsigned long stepsPerSecPerSec)
{
  return (float)(stepsPerSecPerSec & 0x00000FFF) * 14.551915F;
}

// The value in the MAX_SPD register is [(steps/s)*(tick)]/(2^-18) where tick is 
//  250ns (datasheet value)- 0x041 on boot.
// Multiply desired steps/s by .065536 to get an appropriate value for this register
// This is a 10-bit value, so we need to make sure it remains at or below 0x3FF
unsigned long AutoDriver::maxSpdCalc(float stepsPerSec)
{
  unsigned long temp = ceil(stepsPerSec * 0.065536F);
  if( temp > 0x000003FF) return 0x000003FF;
  else return temp;
}


float AutoDriver::maxSpdParse(unsigned long stepsPerSec)
{
    return (float)(stepsPerSec & 0x000003FF) * 15.258789F;
}

// The value in the MIN_SPD register is [(steps/s)*(tick)]/(2^-24) where tick is 
//  250ns (datasheet value)- 0x000 on boot.
// Multiply desired steps/s by 4.1943 to get an appropriate value for this register
// This is a 12-bit value, so we need to make sure the value is at or below 0xFFF.
unsigned long AutoDriver::minSpdCalc(float stepsPerSec)
{
  float temp = stepsPerSec * 4.194304F;
  if( (unsigned long) long(temp) > 0x00000FFF) return 0x00000FFF;
  else return (unsigned long) long(temp);
}

float AutoDriver::minSpdParse(unsigned long stepsPerSec)
{
    return (float) ((float)(stepsPerSec & 0x00000FFF) * 0.23841858F);
}

// The value in the FS_SPD register is ([(steps/s)*(tick)]/(2^-18))-0.5 where tick is 
//  250ns (datasheet value)- 0x027 on boot.
// Multiply desired steps/s by .065536 and subtract .5 to get an appropriate value for this register
// This is a 10-bit value, so we need to make sure the value is at or below 0x3FF.
unsigned long AutoDriver::FSCalc(float stepsPerSec)
{
  float temp = (stepsPerSec * 0.065536F)-0.5F;
  if( (unsigned long) long(temp) > 0x000003FF) return 0x000003FF;
  else return (unsigned long) long(temp);
}

float AutoDriver::FSParse(unsigned long stepsPerSec)
{
    return (((float)(stepsPerSec & 0x000003FF)) + 0.5F) * 15.258789F;
}

// The value in the INT_SPD register is [(steps/s)*(tick)]/(2^-26) where tick is 
//  250ns (datasheet value)- 0x408 on boot.
// Multiply desired steps/s by 16.777216 to get an appropriate value for this register
// This is a 14-bit value, so we need to make sure the value is at or below 0x3FFF.
unsigned long AutoDriver::intSpdCalc(float stepsPerSec)
{
  float temp = stepsPerSec * 16.777216F;
  if( (unsigned long) long(temp) > 0x00003FFF) return 0x00003FFF;
  else return (unsigned long) long(temp);
}

float AutoDriver::intSpdParse(unsigned long stepsPerSec)
{
    return (float)(stepsPerSec & 0x00003FFF) * 0.059604645F;
}

// When issuing RUN command, the 20-bit speed is [(steps/s)*(tick)]/(2^-28) where tick is 
//  250ns (datasheet value).
// Multiply desired steps/s by 67.108864 to get an appropriate value for this register
// This is a 20-bit value, so we need to make sure the value is at or below 0xFFFFF.
unsigned long AutoDriver::spdCalc(float stepsPerSec)
{
  unsigned long temp = stepsPerSec * 67.108864F;
  if( temp > 0x000FFFFF) return 0x000FFFFF;
  else return temp;
}

float AutoDriver::spdParse(unsigned long stepsPerSec)
{
    return (float)(stepsPerSec & 0x000FFFFF) * 0.014901161F;
}

// Register widths in bits. Much of the functionality between "get parameter"
//  and "set parameter" is very similar, so the width table is kept in one
//  place and shared by paramHandler() and the prepare*() methods. Returns 0
//  for an unknown register.
//  This is necessary since not all registers are of the same length, either
//  bit-wise or byte-wise, so we want to make sure we mask out any spurious
//  bits and do the right number of transfers.
byte AutoDriver::paramBitLen(byte param)
{
  switch (param)
  {
    // ABS_POS is the current absolute offset from home. It is a 22 bit number expressed
    //  in two's complement. At power up, this value is 0. It cannot be written when
    //  the motor is running, but at any other time, it can be updated to change the
    //  interpreted position of the motor.
    case ABS_POS:
      return 22;
    // EL_POS is the current electrical position in the step generation cycle. It can
    //  be set when the motor is not in motion. Value is 0 on power up.
    case EL_POS:
      return 9;
    // MARK is a second position other than 0 that the motor can be told to go to. As
    //  with ABS_POS, it is 22-bit two's complement. Value is 0 on power up.
    case MARK:
      return 22;
    // SPEED contains information about the current speed. It is read-only. It does 
    //  NOT provide direction information.
    case SPEED:
      return 20; 
    // ACC and DEC set the acceleration and deceleration rates. 0xFFF is a reserved
    //  value and must not be used (datasheet 9.1.5 and 9.1.6); the valid range is
    //  0x001 to 0xFFE. Use the HARD STOP command to stop with infinite deceleration.
    //  Cannot be written while motor is running. Both default to 0x08A on power up.
    // AccCalc() and DecCalc() functions exist to convert steps/s/s values into
    //  12-bit values for these two registers.
    case ACC: 
      return 12;
    case DECEL: 
      return 12;
    // MAX_SPEED is just what it says- any command which attempts to set the speed
    //  of the motor above this value will simply cause the motor to turn at this
    //  speed. Value is 0x041 on power up.
    // MaxSpdCalc() function exists to convert steps/s value into a 10-bit value
    //  for this register.
    case MAX_SPEED:
      return 10;
    // MIN_SPEED controls two things- the activation of the low-speed optimization
    //  feature and the lowest speed the motor will be allowed to operate at. LSPD_OPT
    //  is the 13th bit, and when it is set, the minimum allowed speed is automatically
    //  set to zero. This value is 0 on startup.
    // MinSpdCalc() function exists to convert steps/s value into a 12-bit value for this
    //  register. SetLSPDOpt() function exists to enable/disable the optimization feature.
    case MIN_SPEED: 
      return 13;
    // FS_SPD register contains a threshold value above which microstepping is disabled
    //  and the dSPIN operates in full-step mode. Defaults to 0x027 on power up.
    // FSCalc() function exists to convert steps/s value into 10-bit integer for this
    //  register.
    case FS_SPD:
      return 10;
    // KVAL is the maximum voltage of the PWM outputs. These 8-bit values are ratiometric
    //  representations: 255 for full output voltage, 128 for half, etc. Default is 0x29.
    // The implications of different KVAL settings is too complex to dig into here, but
    //  it will usually work to max the value for RUN, ACC, and DEC. Maxing the value for
    //  HOLD may result in excessive power dissipation when the motor is not running.
    case KVAL_HOLD:
      return 8;
    case KVAL_RUN:
      return 8;
    case KVAL_ACC:
      return 8;
    case KVAL_DEC:
      return 8;
    // INT_SPD, ST_SLP, FN_SLP_ACC and FN_SLP_DEC are all related to the back EMF
    //  compensation functionality. Please see the datasheet for details of this
    //  function- it is too complex to discuss here. Default values seem to work
    //  well enough.
    case INT_SPD:
      return 14;
    case ST_SLP: 
      return 8;
    case FN_SLP_ACC: 
      return 8;
    case FN_SLP_DEC: 
      return 8;
    // K_THERM is motor winding thermal drift compensation. Please see the datasheet
    //  for full details on operation- the default value should be okay for most users.
    case K_THERM: 
      return 8;
    // ADC_OUT is a read-only register containing the result of the ADC measurements.
    //  This is less useful than it sounds; see the datasheet for more information.
    case ADC_OUT:
      return 8;
    // Set the overcurrent threshold. Ranges from 375mA to 6A in steps of 375mA.
    //  A set of defined constants is provided for the user's convenience. Default
    //  value is 3.375A- 0x08. This is a 4-bit value.
    case OCD_TH: 
      return 8;
    // Stall current threshold. Defaults to 0x40, or 2.03A. Value is from 31.25mA to
    //  4A in 31.25mA steps. This is a 7-bit value.
    case STALL_TH: 
      return 8;
    // STEP_MODE controls the microstepping settings, as well as the generation of an
    //  output signal from the dSPIN. Bits 2:0 control the number of microsteps per
    //  step the part will generate. Bit 7 controls whether the BUSY/SYNC pin outputs
    //  a BUSY signal or a step synchronization signal. Bits 6:4 control the frequency
    //  of the output signal relative to the full-step frequency; see datasheet for
    //  that relationship as it is too complex to reproduce here.
    // Most likely, only the microsteps per step value will be needed; there is a set
    //  of constants provided for ease of use of these values.
    case STEP_MODE:
      return 8;
    // ALARM_EN controls which alarms will cause the FLAG pin to fall. A set of constants
    //  is provided to make this easy to interpret. By default, ALL alarms will trigger the
    //  FLAG pin.
    case ALARM_EN: 
      return 8;
    // CONFIG contains some assorted configuration bits and fields. A fairly comprehensive
    //  set of reasonably self-explanatory constants is provided, but users should refer
    //  to the datasheet before modifying the contents of this register to be certain they
    //  understand the implications of their modifications. Value on boot is 0x2E88; this
    //  can be a useful way to verify proper start up and operation of the dSPIN chip.
    case CONFIG: 
      return 16;
    // STATUS contains read-only information about the current condition of the chip. A
    //  comprehensive set of constants for masking and testing this register is provided, but
    //  users should refer to the datasheet to ensure that they fully understand each one of
    //  the bits in the register.
    case REG_STATUS:  // STATUS is a read-only register
      return 16;
    default:
      return 0;
  }
}

// Value sanitizing applied before a register is written. Read-only registers
//  (SPEED, STATUS) always send zero.
unsigned long AutoDriver::paramMask(byte param, unsigned long value)
{
  switch (param)
  {
    case SPEED:
    case REG_STATUS:
      return 0;
    // K_THERM is a 4-bit value.
    case K_THERM:
    case OCD_TH:
      return value & 0x0F;
    // STALL_TH is a 7-bit value.
    case STALL_TH:
      return value & 0x7F;
    default:
      return value;
  }
}

// Writes or reads one register after its command byte has been sent. For
//  1-byte or smaller transfers of unknown registers, we call SPIXfer()
//  directly.
long AutoDriver::paramHandler(byte param, unsigned long value)
{
  byte bitLen = paramBitLen(param);
  if (bitLen == 0)
  {
    SPIXfer((byte)value);
    return 0;
  }
  return xferParam(paramMask(param, value), bitLen);
}

// Generalization of the subsections of the register read/write functionality.
//  We want the end user to just write the value without worrying about length,
//  so we pass a bit length parameter from the calling function.
long AutoDriver::xferParam(unsigned long value, byte bitLen)
{
  byte byteLen = bitLen/8;      // How many BYTES do we have?
  if (bitLen%8 > 0) byteLen++;  // Make sure not to lose any partial byte values.
  
  byte temp;

  unsigned long retVal = 0; 
  // Interrupt protection is the caller's responsibility (see setParam() and
  //  getParam()), so that the command byte is covered as well.
  for (int i = 0; i < byteLen; i++)
  {
    retVal = retVal << 8;
    temp = SPIXfer((byte)(value>>((byteLen-i-1)*8)));
    retVal |= temp;
  }
  unsigned long mask = 0xffffffff >> (32-bitLen);
  return retVal & mask;
}

byte AutoDriver::SPIXfer(byte data)
{
  if (_numBoards > L6470_MAX_DEVICES) return 0;  // chain larger than supported
  byte dataPacket[L6470_MAX_DEVICES];
  int i;
  for (i=0; i < _numBoards; i++)
  {
    dataPacket[i] = 0;
  }
  dataPacket[_position] = data;
  digitalWrite(_CSPin, LOW);
  _SPI->beginTransaction(SPISettings(_spiClock, MSBFIRST, SPI_MODE3));
  _SPI->transfer(dataPacket, _numBoards);
  _SPI->endTransaction();
  digitalWrite(_CSPin, HIGH);
  return dataPacket[_position];
}

// Sets the SPI clock used for every transfer. The datasheet maximum is 5 MHz.
void AutoDriver::setSPIClock(uint32_t hz)
{
  if (hz == 0) return;
  if (hz > 5000000) hz = 5000000;
  _spiClock = hz;
}
