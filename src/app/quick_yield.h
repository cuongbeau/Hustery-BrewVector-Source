#pragma once
#include <stdint.h>
namespace QuickYield {
void poll(); void reset();
bool command(uint8_t operation, uint32_t token);
bool editing(); void rotate(int steps);
float displayed(); uint32_t token(); uint8_t flags();
void beginShot(bool brew); void endShot();
float active(); // Whole-shot grams; NAN: unchanged; zero: no final yield stop.
}
