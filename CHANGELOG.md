# Changelog

## v1.2.0 (Unreleased)

### Behavior changes
- `getAcc()` / `getDec()` now return values about 4.9% smaller than v1.1.0. v1.1.0 used the MAX_SPEED / FS_SPD factor by mistake; the new values match the datasheet.
- `setAcc()` / `setDec()` round to the nearest register value instead of truncating, so the written value may differ by 1 LSB (e.g. `setAcc(1000)` writes 69 instead of 68). Both clamp to 0x001-0xFFE.
- A chain longer than `L6470_MAX_DEVICES` (default 16) is no longer supported; `SPIXfer()` does nothing in that case. Define `L6470_MAX_DEVICES` before including the library to raise the limit.

### Fixed
- ACC/DEC read-back factor (1 LSB = 14.551915 steps/s/s). `getDec()` now uses `decParse()`.
- `setDec()` no longer writes the reserved value 0xFFF.
- `goUntilRaw()` clamps the speed to 20 bits (0xFFFFF) like `runRaw()`. Previously values from 0x100000 to 0x3FFFFF were sent as is; the chip ignored the upper bits, so a too-large speed could result in a very low speed.
- On SAMD, interrupts are disabled for the whole SPI transaction (command and data bytes) of `setParam()`, `getParam()`, `getStatus()` and multi-byte motion commands. PRIMASK is saved and restored, so the protection nests safely.
- Include guards are unique (`PONOOR_L6470_CONSTANTS_H`, `PONOOR_L6470_LIBRARY_H`). The old constants guard clashed with Ponoor_PowerSTEP01_Library.
- Sign extension in `getPos()` / `getMark()` no longer depends on the size of `long`.
- Examples include `Ponoor_L6470Library.h`. In v1.1.0 they still referred to `SparkFunAutoDriver.h` and did not compile.
- Stale comments on conversion factors and the removed "infinite acceleration" mode.

### Added
- Packed daisy-chain transfers: `prepare*()` methods, `AutoDriver::performPrepared()` (returns `false` and sends nothing if the prepared devices do not share the same CS pin and SPI port), `preparedResult()`, `preparedPos()` and `preparedStatus()`.
- `AutoDriver::setSPIClock()` (default 4 MHz, clamped to 5 MHz).
- `PackedBasics` and `PackedCommands` examples.
- Host-side tests in `test/`.

### Changed
- Example folders and files no longer use SparkFun names.
- `SPIXfer()` uses a fixed-size buffer instead of a variable-length array.
- Command assembly and the register width table are shared between the immediate and packed APIs.
- `keywords.txt` updated; README rewritten; `library.properties` description updated.

## v1.1.0 (2024-09-20)
- Fixed unit conversion factors according to the datasheet:
  - ACC/DEC: written values were twice the correct value.
  - INT_SPD: written values were a quarter of the correct value.
  - MIN_SPEED and RUN speed: small corrections (about 0.2% and 0.004%).
- `setAcc()` clamps to 0xFFE because 0xFFF is reserved.
- Known issue: `getAcc()` / `getDec()` used a wrong factor (fixed in v1.2.0).

## v1.0.3 (2021-10-14)
- Updated `keywords.txt`.

## v1.0.2 (2021-09-01)
- Updated `library.properties`.

## v1.0.1 (2021-08-31)
- Added `getElPos()` / `setElPos()`.

## v1.0.0 (2021-04-24)
- Forked from the SparkFun AutoDriver Arduino Library.
- Added `getSpeed()` and raw register accessors (`set/get*Raw()`, `runRaw()`, `goUntilRaw()`).
- Disabled interrupts during `getStatus()` and `xferParam()` on SAMD.
- Renamed command and status constants (`CMD_*`, `REG_STATUS`) to avoid conflicts with other libraries.
- Fixed the bit shift of `STATUS_MOT_STATUS_*` constants.
- `goUntil()` / `releaseSw()` treat any non-zero `action` as COPY.
