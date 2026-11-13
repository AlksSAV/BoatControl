// BoatControl — руль: карта оси X в импульс сервы (SPEC п.3).
// sign>0 = вправо (к 2000), sign<0 = влево (к 1000).
// Инверсия оси STB_INVERT применяется вызывающим кодом (main).

#include "steering.h"
#include <math.h>

uint16_t Steering::map_pulse(int32_t offset_adc) {
    int32_t mag = (offset_adc >= 0) ? offset_adc : -offset_adc;

    if (mag <= X_DEADBAND_ADC) return RC_PULSE_NEUTRAL;

    // нормализация 0..1
    float t = (float)(mag - X_DEADBAND_ADC) / (float)(X_SPAN_ADC - X_DEADBAND_ADC);
    if (t > 1.0f) t = 1.0f;

    // экспонента у центра (SPEC п.3)
    t = powf(t, X_EXPONENT);

    int32_t d = (int32_t)(t * RC_PULSE_SPAN + 0.5f);
    int32_t pulse = RC_PULSE_NEUTRAL + ((offset_adc > 0) ? d : -d);

    if (pulse < STB_PULSE_MIN) pulse = STB_PULSE_MIN;
    if (pulse > STB_PULSE_MAX) pulse = STB_PULSE_MAX;
    return (uint16_t)pulse;
}

// Ход сервы ограничиваем скоростью, а не формой кривой: полный ход упор-в-упор
// (STB_PULSE_MIN..STB_PULSE_MAX) занимает STB_SLEW_FULL_MS, поэтому за dt_ms
// проходим (STB_PULSE_MAX-STB_PULSE_MIN)*dt/STB_SLEW_FULL_MS мкс. Дробный шаг
// округляем вверх (минимум 1 мкс), иначе на малом dt серва встаёт и цели не
// достигает.
void Steering::slew_to(uint16_t target_us, uint16_t dt_ms) {
    uint16_t diff = (target_us > _out) ? (uint16_t)(target_us - _out)
                                       : (uint16_t)(_out - target_us);
    if (diff == 0) return;

    uint32_t step = (uint32_t)(STB_PULSE_MAX - STB_PULSE_MIN) * dt_ms / STB_SLEW_FULL_MS;
    if (step == 0) step = 1;
    if (step > diff) step = diff;

    _out = (target_us > _out) ? (uint16_t)(_out + step)
                              : (uint16_t)(_out - step);
}
