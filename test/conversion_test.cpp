// Host-side test for unit conversion functions.
// Build: g++ -std=c++11 -Itest/mock -Isrc test/conversion_test.cpp src/*.cpp -o /tmp/conversion_test
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Arduino.h"
#include "SPI.h"
#define private public
#include "Ponoor_L6470Library.h"
#undef private

SPIClass SPI;
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

int main()
{
  AutoDriver d(0, 10, 6);

  int roundTripErrors = 0;
  for (unsigned long n = 1; n <= 0xFFE; n++) {
    if (d.accCalc(d.accParse(n)) != n) roundTripErrors++;
    if (d.decCalc(d.decParse(n)) != n) roundTripErrors++;
  }
  printf("round trip errors: %d\n", roundTripErrors);
  CHECK(roundTripErrors == 0);

  CHECK(fabs(d.accParse(68) - 989.5f) < 0.1f);
  CHECK(fabs(d.decParse(68) - 989.5f) < 0.1f);
  CHECK(d.accCalc(1e6f) == 0xFFE);
  CHECK(d.decCalc(1e6f) == 0xFFE);
  CHECK(d.accCalc(0.0f) == 1);
  CHECK(d.decCalc(0.0f) == 1);
  CHECK(d.accCalc(-5.0f) == 1);
  CHECK(d.accCalc(59590.0f) == 0xFFE);

  printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
