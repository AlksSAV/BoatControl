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
