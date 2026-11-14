// BoatControl — руль: карта оси X в импульс сервы (SPEC п.3).
// sign>0 = вправо (к 2000), sign<0 = влево (к 1000).
// Инверсия оси STB_INVERT применяется вызывающим кодом (main).

#include "steering.h"
#include "params.h"
#include <math.h>

uint16_t Steering::map_pulse(int32_t offset_adc) {
    int32_t mag = (offset_adc >= 0) ? offset_adc : -offset_adc;

    if (mag <= g_params.x_deadband_adc) return RC_PULSE_NEUTRAL;

    // нормализация 0..1
    float t = (float)(mag - g_params.x_deadband_adc) /
              (float)(g_params.x_span_adc - g_params.x_deadband_adc);
    if (t > 1.0f) t = 1.0f;

    // экспонента у центра (SPEC п.3)
    t = powf(t, g_params.x_exponent);

    int32_t d = (int32_t)(t * RC_PULSE_SPAN + 0.5f);
    int32_t pulse = RC_PULSE_NEUTRAL + ((offset_adc > 0) ? d : -d);

    if (pulse < g_params.stb_pulse_min) pulse = g_params.stb_pulse_min;
    if (pulse > g_params.stb_pulse_max) pulse = g_params.stb_pulse_max;
    return (uint16_t)pulse;
}

// Ход сервы ограничиваем скоростью, а не формой кривой: полный ход упор-в-упор
// (stb_pulse_min..stb_pulse_max) занимает stb_slew_full_ms, поэтому за dt_ms
// проходим (stb_pulse_max-stb_pulse_min)*dt/stb_slew_full_ms мкс. Дробный шаг
// округляем вверх (минимум 1 мкс), иначе на малом dt серва встаёт и цели не
// достигает.
void Steering::slew_to(uint16_t target_us, uint16_t dt_ms) {
    uint16_t diff = (target_us > _out) ? (uint16_t)(target_us - _out)
                                       : (uint16_t)(_out - target_us);
    if (diff == 0) return;

    uint32_t step = (uint32_t)(g_params.stb_pulse_max - g_params.stb_pulse_min) * dt_ms /
                    g_params.stb_slew_full_ms;
    if (step == 0) step = 1;
    if (step > diff) step = diff;

    _out = (target_us > _out) ? (uint16_t)(_out + step)
                              : (uint16_t)(_out - step);
}
