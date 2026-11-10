// BoatControl — заглушка Arduino.h для нативных (PC) юнит-тестов.
// Даёт только то, что реально нужно логике: целочисленные типы и abs().
// GPIO/LEDC/ADC не эмулируются: они живут в hal.cpp, который в native-тесты
// не линкуется (см. build_src_filter в platformio.ini).

#ifndef BOAT_MOCK_ARDUINO_H
#define BOAT_MOCK_ARDUINO_H

#include <cstdint>
#include <cstdlib>

#endif // BOAT_MOCK_ARDUINO_H
