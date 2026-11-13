// BoatControl — руль (SPEC п.3): карта оси X в импульс сервы.

#ifndef BOAT_STEERING_H
#define BOAT_STEERING_H

#include <Arduino.h>
#include "config.h"

class Steering {
  public:
    // Смещение оси X от нуля (со знаком) -> импульс сервы 1000..2000.
    static uint16_t map_pulse(int32_t offset_adc);

    // Протянуть текущий выход к цели за dt_ms (аналог Throttle::slew_to).
    void slew_to(uint16_t target_us, uint16_t dt_ms);

    uint16_t out() const { return _out; }

    // Форсировать значение (BOOT/CALIBRATE/FAULT).
    void force(uint16_t us) { _out = us; }

  private:
    uint16_t _out = RC_PULSE_NEUTRAL;
};

#endif
