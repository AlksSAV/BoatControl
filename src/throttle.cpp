// BoatControl — газ: карта + rate limit + пауза смены знака (SPEC п.3, п.6).

#include "throttle.h"
#include "params.h"

uint16_t Throttle::map_pulse(int32_t offset_adc) {
    // offset_adc > 0 = стик «от себя» = вперёд = 1000..1500
    // offset_adc < 0 = стик «на себя» = назад   = 1500..2000
    int32_t mag = (offset_adc >= 0) ? offset_adc : -offset_adc;

    if (mag <= g_params.thr_deadband_adc) return RC_PULSE_NEUTRAL;

    uint32_t d = (uint32_t)(mag - g_params.thr_deadband_adc) * RC_PULSE_SPAN
                 / (g_params.thr_span_adc - g_params.thr_deadband_adc);
    if (d > RC_PULSE_SPAN) d = RC_PULSE_SPAN;

    if (offset_adc > 0) return (uint16_t)(RC_PULSE_NEUTRAL - (int32_t)d);  // вперёд
    return (uint16_t)(RC_PULSE_NEUTRAL + (int32_t)d);                      // назад
}

// Макс. изменение за dt_ms при rate limit (SPEC п.3: набор медленнее сброса).
static uint16_t rate_limit_us(uint16_t cur_mag, uint16_t tgt_mag, uint16_t dt_ms) {
    uint8_t pct = (tgt_mag > cur_mag) ? g_params.thr_rate_up_pct
                                      : g_params.thr_rate_down_pct;
    return (uint16_t)((uint32_t)RC_PULSE_SPAN * pct * dt_ms / 10000UL);
}

void Throttle::slew_to(uint16_t target_us, uint16_t dt_ms, uint16_t cap_us) {
    // Пауза в нейтрали при смене знака (SPEC п.3): держим 1500 мкс.
    if (_zero_hold_ms > 0) {
        _zero_hold_ms = (_zero_hold_ms > dt_ms) ? (_zero_hold_ms - dt_ms) : 0;
        return;
    }

    int sign_cur = sign_of(_out);
    int sign_tgt = sign_of(target_us);

    // Смена знака: форсируем нейтраль и начинаем отсчёт паузы.
    if (sign_cur != 0 && sign_tgt != 0 && sign_cur != sign_tgt) {
        _out = RC_PULSE_NEUTRAL;
        _zero_hold_ms = g_params.thr_zero_pass_ms;
        return;
    }

    uint16_t cur_mag = (uint16_t)abs((int32_t)_out - RC_PULSE_NEUTRAL);
    uint16_t tgt_mag = (uint16_t)abs((int32_t)target_us - RC_PULSE_NEUTRAL);

    uint16_t allowed = rate_limit_us(cur_mag, tgt_mag, dt_ms);
    if (cap_us > 0 && cap_us < allowed) allowed = cap_us;

    uint16_t diff = (tgt_mag > cur_mag) ? (tgt_mag - cur_mag) : (cur_mag - tgt_mag);
    uint16_t step = (diff > allowed) ? allowed : diff;

    uint16_t new_mag = (tgt_mag > cur_mag) ? (cur_mag + step) : (cur_mag - step);
    int sign = sign_tgt;

    if (sign < 0) _out = (uint16_t)(RC_PULSE_NEUTRAL - new_mag);       // вперёд
    else if (sign > 0) _out = (uint16_t)(RC_PULSE_NEUTRAL + new_mag);  // назад
    else _out = RC_PULSE_NEUTRAL;
}
