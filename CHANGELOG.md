# Changelog

## v1.2.0

### Breaking-ish
- `getAcc()` / `getDec()` return values about 4.9% smaller than before. The previous values were too large because the conversion factor for MAX_SPEED / FS_SPD was used by mistake; the new values are correct. Code that compared or stored the old readings may need adjusting. `setAcc()` / `setDec()` now write ACC/DEC register values that are correct for the requested steps/s/s (rounded to the nearest LSB instead of truncated).

### Fixed
- ACC/DEC conversion now uses the datasheet factors (1 LSB = 14.551915 steps/s/s). `decParse()` is used by `getDec()`.
- `accCalc()` / `decCalc()` clamp to 0x001-0xFFE. 0xFFF is reserved and no longer produced by `setDec()`.
- Stale comments about ACC/DEC, INT_SPD and RUN conversion factors, and the removed "infinite acceleration" mode, were corrected.
- On SAMD, interrupts are now disabled for the whole SPI transaction (command byte and data bytes) of `setParam()`, `getParam()`, `getStatus()` and the multi-byte motion commands. The protection saves and restores PRIMASK, so it nests safely.
- Include guards are unique (`PONOOR_L6470_CONSTANTS_H`, `PONOOR_L6470_LIBRARY_H`); the old constants guard clashed with Ponoor_PowerSTEP01_Library.
- `getPos()` / `getMark()` sign extension no longer depends on the size of `long`.

- `goUntilRaw()` clamps the speed to 20 bits (0xFFFFF) like `runRaw()`. Previously values from 0x100000 to 0x3FFFFF were sent as is, and the chip ignored the upper bits, so a too-large speed could end up as MIN_SPEED.

### Added
- Packed daisy-chain transfers: `prepareGetParam()`, `prepareSetParam()`, `prepareGetStatus()`, `prepareGetPos()`, `prepareRun()`, `prepareRunRaw()`, `prepareMove()`, `prepareGoTo()`, `prepareGoToDir()`, `prepareSoftStop()`, `prepareHardStop()`, `prepareSoftHiZ()`, `prepareHardHiZ()`, `prepareNop()`, `AutoDriver::performPrepared()`, `preparedResult()`, `preparedPos()`, `preparedStatus()`. `L6470_MAX_DEVICES` sets the maximum chain length (default 16).
- `AutoDriver::setSPIClock()` (default 4 MHz, clamped to 5 MHz).
- `PackedBasics` and `PackedCommands` examples.
- `keywords.txt` covers the raw accessors, `getSpeed()`, EL_POS functions, packed transfer methods and `CMD_*` constants.
- Host-side tests in `test/`.
- `CHANGELOG.md`.

### Changed
- `SPIXfer()` uses a fixed-size buffer instead of a variable-length array.
- Command assembly and the register width table are shared between the immediate and the packed API.
- README rewritten; `library.properties` description updated.

## v1.1.0 (2024-09)
- Fixed unit conversion factors for speed registers (MAX_SPEED, MIN_SPEED, FS_SPD, INT_SPD and RUN speed).

## v1.0.0 (2021)
- Forked from the SparkFun AutoDriver library. Added SAMD support, `getSpeed()`, raw register accessors and EL_POS functions, and renamed some constants to avoid conflicts with other libraries.
