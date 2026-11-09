#include "stick.h"
#include "hal.h"

uint16_t Stick::update() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES_PER_LOOP; i++) {
        sum += hal_adc(_pin);
    }
    _mean = (uint16_t)(sum / ADC_SAMPLES_PER_LOOP);

    // кольцевой буфер средних за ~1 с
    _ring[_ring_i] = _mean;
    _ring_i = (uint8_t)((_ring_i + 1) % WINDOW);
    if (_ring_n < WINDOW) _ring_n++;

    return _mean;
}

bool Stick::noisy() const {
    if (_ring_n < WINDOW) return false;  // окно ещё не набрано
    uint16_t mn = ADC_MAX_RAW, mx = 0;
    for (uint8_t i = 0; i < WINDOW; i++) {
        if (_ring[i] < mn) mn = _ring[i];
        if (_ring[i] > mx) mx = _ring[i];
    }
    return (uint16_t)(mx - mn) > ADC_NOISE_MAX;
}

void Stick::cal_reset() {
    _cal_sum = 0;
    _cal_n = 0;
    _cal_min = 0xFFFF;
    _cal_max = 0;
    _calibrated = false;
}

void Stick::cal_add() {
    update();
    if (_mean < _cal_min) _cal_min = _mean;
    if (_mean > _cal_max) _cal_max = _mean;
    _cal_sum += _mean;
    _cal_n++;
}

bool Stick::cal_finish() {
    if (_cal_n < CAL_SAMPLES) return false;

    uint16_t center = (uint16_t)(_cal_sum / _cal_n);

    // Допуски (SPEC п.5): смещение нуля, валидность, разброс при покое.
    bool ok = (abs((int32_t)center - ADC_MID_RAW) <= CAL_MAX_OFFSET_ADC)
           && (center >= ADC_VALID_MIN && center <= ADC_VALID_MAX)
           && ((uint16_t)(_cal_max - _cal_min) <= CAL_MAX_NOISE_ADC);

    if (ok) {
        _center = center;
        _calibrated = true;
    }
    return ok;
}
