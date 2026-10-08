// Host-side test for packed daisy-chain transfers. A small simulator of an
// L6470 chain sits behind a mocked SPI port; the same operations are run
// through the immediate API and the prepare*()/performPrepared() API and the
// resulting chip state / read values are compared.
// Build: g++ -std=c++11 -Itest/mock -Isrc test/packed_test.cpp src/*.cpp -o /tmp/packed_test
#include <cstdio>
#include <vector>
#include <string>
#include "Arduino.h"
#include "SPI.h"
#include "Ponoor_L6470Library.h"

SPIClass SPI;

static int csLowCount = 0;
void digitalWrite(int pin, int value) { if (pin == 10 && value == LOW) csLowCount++; }
int digitalRead(int) { return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

// ---- minimal L6470 model -------------------------------------------------
struct Chip {
  int32_t absPos = 0;
  uint16_t status = 0x7E03;
  uint16_t maxSpeed = 0x41;
  byte cmd = 0;
  int remaining = 0;
  uint32_t acc = 0;
  byte reply[4] = {0, 0, 0, 0};
  int replyIdx = 0;
  std::vector<std::string> log;

  static int dataBytes(byte c) {
    if (c == 0xD0) return 2;
    if ((c & 0xFE) == 0x50 || (c & 0xFE) == 0x40 || (c & 0xFE) == 0x60 || (c & 0xFE) == 0x68) return 3;
    if ((c & 0xE0) == 0x20 || ((c & 0xE0) == 0x00 && c != 0)) {
      switch (c & 0x1F) { case 0x01: return 3; case 0x07: return 2; case 0x19: return 2; default: return 1; }
    }
    return 0;
  }
  byte xfer(byte in) {
    byte out = 0xAA;  // garbage
    if (remaining == 0) {
      cmd = in; acc = 0; replyIdx = 0;
      remaining = dataBytes(in);
      for (int i = 0; i < 4; i++) reply[i] = 0;
      if ((in & 0xE0) == 0x20) {
        uint32_t v = 0;
        if ((in & 0x1F) == 0x01) v = (uint32_t)absPos & 0x3FFFFF;
        else if ((in & 0x1F) == 0x07) v = maxSpeed;
        else if ((in & 0x1F) == 0x19) v = status;
        for (int i = 0; i < remaining; i++) reply[i] = (v >> ((remaining - 1 - i) * 8)) & 0xFF;
      } else if (in == 0xD0) {
        for (int i = 0; i < 2; i++) reply[i] = (status >> ((1 - i) * 8)) & 0xFF;
      }
      if (remaining == 0 && in != 0) log.push_back("cmd " + std::to_string(in));
      return out;
    }
    acc = (acc << 8) | in;
    out = reply[replyIdx++];
    if (--remaining == 0) {
      if ((cmd & 0xE0) == 0x00 && cmd == 0x01) absPos = (acc & 0x200000) ? (int32_t)(acc | 0xFFC00000) : (int32_t)acc;
      else if ((cmd & 0xE0) == 0x00) log.push_back("set " + std::to_string(cmd) + "=" + std::to_string(acc));
      else if ((cmd & 0xE0) != 0x20 && cmd != 0xD0) log.push_back("cmd " + std::to_string(cmd) + " " + std::to_string(acc));
    }
    return out;
  }
};

static const int N = 4;
static Chip chips[N];
static int frames = 0;

static void chainTransfer(uint8_t *buf, size_t n) {
  frames++;
  for (size_t i = 0; i < n; i++) buf[i] = chips[i].xfer(buf[i]);
}

int main()
{
  SPI.hook = chainTransfer;
  AutoDriver d0(0, 10, 6), d1(1, 10, 6), d2(2, 10, 6), d3(3, 10, 6);
  AutoDriver *drv[N] = { &d0, &d1, &d2, &d3 };
  int32_t pos[N] = { 12345, -1234, 0, -2097151 };
  for (int i = 0; i < N; i++) chips[i].absPos = pos[i];
  chips[1].status = 0x1234;

  // --- immediate reference ---
  long refPos[N]; int refStatus[N];
  frames = 0;
  for (int i = 0; i < N; i++) refPos[i] = drv[i]->getPos();
  int immediateFrames = frames;
  for (int i = 0; i < N; i++) refStatus[i] = drv[i]->getStatus();
  for (int i = 0; i < N; i++) { CHECK(refPos[i] == pos[i]); if (refPos[i] != pos[i]) printf("  i=%d ref=%ld exp=%d\n", i, refPos[i], (int)pos[i]); }

  // --- packed reads ---
  frames = 0; csLowCount = 0;
  for (int i = 0; i < N; i++) drv[i]->prepareGetPos();
  CHECK(frames == 0);  // preparing does not touch SPI
  CHECK(AutoDriver::performPrepared());
  CHECK(frames == 4);
  CHECK(csLowCount == 4);
  for (int i = 0; i < N; i++) CHECK(drv[i]->preparedPos() == refPos[i]);
  printf("getPos: immediate %d frames, packed %d frames\n", immediateFrames, frames);

  for (int i = 0; i < N; i++) drv[i]->prepareGetStatus();
  CHECK(AutoDriver::performPrepared());
  for (int i = 0; i < N; i++) CHECK(drv[i]->preparedStatus() == refStatus[i]);
  CHECK(drv[1]->preparedStatus() == 0x1234);

  // register read of a 10-bit register is masked
  chips[2].maxSpeed = 0x3FF;
  for (int i = 0; i < N; i++) drv[i]->prepareGetParam(MAX_SPEED);
  CHECK(AutoDriver::performPrepared());
  CHECK(drv[2]->preparedResult() == 0x3FF);
  CHECK(drv[0]->preparedResult() == 0x41);

  // --- mixed commands: compare against immediate execution ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->run(FWD, 1000.0f);
  drv[1]->move(REV, 5000);
  drv[2]->goTo(-100);
  drv[3]->softStop();
  drv[0]->setParam(MAX_SPEED, 0x55);
  std::vector<std::string> ref[N];
  for (int i = 0; i < N; i++) { ref[i] = chips[i].log; chips[i].log.clear(); }

  frames = 0;
  drv[0]->prepareRun(FWD, 1000.0f);
  drv[1]->prepareMove(REV, 5000);
  drv[2]->prepareGoTo(-100);
  drv[3]->prepareSoftStop();
  CHECK(AutoDriver::performPrepared());
  CHECK(frames == 4);
  drv[0]->prepareSetParam(MAX_SPEED, 0x55);
  CHECK(AutoDriver::performPrepared());
  CHECK(frames == 7);
  for (int i = 0; i < N; i++) {
    CHECK(chips[i].log == ref[i]);
    if (chips[i].log != ref[i]) {
      for (auto &l : chips[i].log) printf("  packed[%d]: %s\n", i, l.c_str());
      for (auto &l : ref[i]) printf("  ref[%d]:    %s\n", i, l.c_str());
    }
  }

  // --- cancel with prepareNop and nothing staged ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->prepareRun(FWD, 1000.0f);
  drv[0]->prepareNop();
  frames = 0;
  CHECK(AutoDriver::performPrepared());
  CHECK(frames == 0);
  CHECK(chips[0].log.empty());
  // prepared state is cleared after perform
  drv[1]->prepareHardStop();
  CHECK(AutoDriver::performPrepared());
  frames = 0;
  CHECK(AutoDriver::performPrepared());
  CHECK(frames == 0);

  // --- SPI clock clamp ---
  AutoDriver::setSPIClock(10000000);
  drv[0]->getPos();
  CHECK(SPI.lastClock == 5000000);
  AutoDriver::setSPIClock(1000000);
  drv[0]->getPos();
  CHECK(SPI.lastClock == 1000000);
  AutoDriver::setSPIClock(4000000);

  // --- an instance on another CS pin makes the chain unsendable ---
  AutoDriver odd(4, 11, 6);
  d0.prepareHardStop();
  frames = 0;
  CHECK(!AutoDriver::performPrepared());
  CHECK(frames == 0);

  printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
