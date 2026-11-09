// BoatControl — газ (SPEC п.3, п.6): карта оси Y в импульс ESC,
// rate limit и обязательный проход через нейтраль при смене знака.

#ifndef BOAT_THROTTLE_H
#define BOAT_THROTTLE_H

#include <Arduino.h>
#include "config.h"

class Throttle {
  public:
    // Смещение оси Y от нуля (со знаком, АЦП-единицы) -> целевой импульс.
    // offset>0 (стик «от себя») = вперёд = 1500..1000; offset<0 = назад = 1500..2000.
    static uint16_t map_pulse(int32_t offset_adc);

    // Протянуть текущий выход к цели за dt_ms.
    // cap_us (0 = без ограничения) — доп. потолок за такт, для возврата из круиза.
    void slew_to(uint16_t target_us, uint16_t dt_ms, uint16_t cap_us = 0);

    uint16_t out() const { return _out; }

    // Форсировать значение (BOOT/CALIBRATE/FAULT).
    void force(uint16_t us) { _out = us; _zero_hold_ms = 0; }

    // Знак: -1 вперёд (us<нейтраль), +1 назад, 0 нейтраль.
    static int sign_of(uint16_t us) {
        if (us < RC_PULSE_NEUTRAL) return -1;
        if (us > RC_PULSE_NEUTRAL) return 1;
        return 0;
    }

    // Газ в процентах полного хода (0..100).
    static uint8_t percent(uint16_t us) {
        uint16_t d = (us > RC_PULSE_NEUTRAL) ? (us - RC_PULSE_NEUTRAL)
                                             : (RC_PULSE_NEUTRAL - us);
        return (uint8_t)((uint32_t)d * 100UL / RC_PULSE_SPAN);
    }

  private:
    uint16_t _out = RC_PULSE_NEUTRAL;
    uint16_t _zero_hold_ms = 0;  // остаток паузы в нейтрали
};

#endif
