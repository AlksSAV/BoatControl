// BoatControl — руль (SPEC п.3): карта оси X в импульс сервы.

#ifndef BOAT_STEERING_H
#define BOAT_STEERING_H

#include <Arduino.h>
#include "config.h"

class Steering {
  public:
    // Смещение оси X от нуля (со знаком) -> импульс сервы 1000..2000.
    static uint16_t map_pulse(int32_t offset_adc);
};

#endif
