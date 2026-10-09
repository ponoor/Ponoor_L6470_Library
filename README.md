Ponoor L6470 Library
====================

Arduino library for the STMicroelectronics [L6470](https://www.st.com/en/motor-drivers/l6470.html) (dSPIN) stepper motor driver.

This library is a fork of the L6470-based SparkFun [AutoDriver library](https://github.com/sparkfun/SparkFun_AutoDriver_Arduino_Library). It is used in Ponoor products such as the STEP800 stepper motor driver board, and works on SAMD (ARM Cortex-M0+) boards as well as other Arduino architectures.

Repository Contents
-------------------
* **src** - Source of the Arduino library.
* **examples** - Example sketches demonstrating the use of the library.
* **test** - Host-side tests (not part of the Arduino build). See [Running the tests](#running-the-tests).
* **keywords.txt** - List of words to be highlighted by the Arduino IDE.
* **library.properties** - Used by the Arduino package manager.
* **CHANGELOG.md** - Release history.

Daisy chain
-----------
Several L6470 chips can be connected in a daisy chain that shares one CS pin. Every `AutoDriver` instance represents one chip:

```cpp
AutoDriver board0(0, 10, 6);  // position 0, CS pin 10, reset pin 6
AutoDriver board1(1, 10, 6);  // position 1, same CS pin
```

The first argument, `position`, is the position of the chip in the chain, counted from 0 at the chip **farthest** from the controller (the end of the chain). All instances in the chain must share the same CS pin, and every instance must be constructed before the first transfer, because the library needs to know the length of the chain.

### Packed transfers

With the regular API, every call shifts the whole chain: each byte of a command is sent as a frame of N bytes (the byte for the target chip and NOP for all the others). Sending one command to each of N chips therefore costs N x N bytes per byte of command.

A frame of a daisy chain can carry a different byte for every chip, so the commands of all chips can be sent together. The `prepare*()` methods stage a command in an instance without any SPI traffic. `AutoDriver::performPrepared()` then sends the staged commands of all instances in at most 4 frames (the maximum length of a command), and the responses are read back with `preparedPos()`, `preparedStatus()` or `preparedResult()`.

```cpp
// Read the position of every motor with one transfer...
for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareGetPos();
AutoDriver::performPrepared();

// ...and update the speed of every motor with another one.
for (int i = 0; i < NUM_BOARDS; i++) {
  long pos = boards[i]->preparedPos();
  boards[i]->prepareRun(pos < target ? FWD : REV, 500);
}
AutoDriver::performPrepared();
```

Available methods: `prepareGetParam()`, `prepareSetParam()`, `prepareGetStatus()`, `prepareGetPos()`, `prepareRun()`, `prepareRunRaw()`, `prepareMove()`, `prepareGoTo()`, `prepareGoToDir()`, `prepareSoftStop()`, `prepareHardStop()`, `prepareSoftHiZ()`, `prepareHardHiZ()` and `prepareNop()` (cancels the staged command).

Notes and restrictions:

* Only one command can be staged per instance; preparing again overwrites it. Instances without a staged command receive NOP.
* The results stay valid until the next `prepare*()` call on the same instance.
* Packed transfers require a **single chain**: all instances must share the same CS pin and the same SPI port. Otherwise (or if positions are duplicated or out of range) `performPrepared()` sends nothing and returns `false`.
* The maximum number of instances is `L6470_MAX_DEVICES` (default 16). Define it before including the library to change it.
* `AutoDriver::setSPIClock(hz)` changes the SPI clock (default 4 MHz, clamped to the datasheet maximum of 5 MHz).

Differences from the original library
-------------------------------------
- Added `getSpeed()`.
- Added raw register accessors (`set/get*Raw()`, `runRaw()`, `goUntilRaw()`).
- Added `getElPos()` / `setElPos()`.
- Added packed daisy-chain transfers (`prepare*()` / `performPrepared()`) and `setSPIClock()`.
- Fixed unit conversion factors for ACC, DEC, INT_SPD, and the RUN speed according to the datasheet. ACC/DEC are clamped to 0x001-0xFFE (0xFFF is reserved).
- Fixed the bit shift of the `STATUS_MOT_STATUS_*` constants.
- `goUntil()` / `releaseSw()` accept any non-zero `action` as COPY.
- Interrupts are disabled during SPI transactions on SAMD to prevent corrupted return values.
- Renamed command and status constants (`CMD_*`, `REG_STATUS`) to avoid conflicts with other libraries.
- Unique include guards.

Examples
--------
* **AutoDriverLibraryTest** - Plays music with stepper motors on a chain of boards.
* **L6470_dSPIN_Example** - Basic example using the L6470 directly, without the AutoDriver class (AVR only: uses Timer1 registers).
* **ParameterGetSetTest** - Checks that values read back after being set are consistent.
* **gantry** - Controls a five-axis gantry on one daisy chain.
* **PackedBasics** - Minimal packed-transfer example: read positions/status and send a different command to each board, controlled from the Serial Monitor.
* **PackedCommands** - Compares the regular and the packed API on an 8-chip chain and runs a simple P-control servo loop.

Running the tests
-----------------
The tests run on the host with mocked Arduino and SPI headers:

```
g++ -std=c++11 -Itest/mock -Isrc test/conversion_test.cpp src/*.cpp -o conversion_test && ./conversion_test
g++ -std=c++11 -Itest/mock -Isrc test/packed_test.cpp src/*.cpp -o packed_test && ./packed_test
```

License Information
-------------------
This product is open source!

The code is beerware; if you see any SparkFun employee at the local, and you've found their code helpful, please buy them a round!

Please use, reuse, and modify these files as you see fit. Please maintain attribution to SparkFun Electronics and release anything derivative under the same license.

Distributed as-is; no warranty is given.

- Your friends at SparkFun.
