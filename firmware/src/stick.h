// BoatControl — ось джойстика: oversampling, тарировка нуля, контроль валидности.

#ifndef BOAT_STICK_H
#define BOAT_STICK_H

#include <Arduino.h>
#include "config.h"

class Stick {
  public:
    explicit Stick(uint8_t pin) : _pin(pin) {}

    // Один такт: ADC_SAMPLES_PER_LOOP отсчётов -> среднее. Возвращает среднее.
    uint16_t update();

    // Смещение от тарированного нуля, со знаком. Инверсию оси применяет вызывающий.
    int32_t offset() const { return (int32_t)_mean - (int32_t)_center; }

    // Среднее вне ADC_VALID_MIN..MAX (SPEC п.7).
    bool broken() const { return _mean < ADC_VALID_MIN || _mean > ADC_VALID_MAX; }

    // Разброс средних за ~1 с выше ADC_NOISE_MAX (SPEC п.7).
    bool noisy() const;

    // Тарировка по шагам цикла (не блокирующая): reset -> add() каждый такт -> finish().
    void cal_reset();
    void cal_add();
    bool cal_finish();  // true = допуск пройден
    uint8_t cal_n_done() const { return _cal_n; }

    bool calibrated() const { return _calibrated; }
    uint16_t center() const { return _center; }
    uint16_t last() const { return _mean; }

  private:
    static constexpr uint8_t WINDOW = 1000 / LOOP_PERIOD_MS;  // ~1 с средних

    uint8_t _pin;
    uint16_t _mean = ADC_MID_RAW;
    uint16_t _center = ADC_MID_RAW;
    bool _calibrated = false;

    uint16_t _ring[WINDOW] = {0};
    uint8_t _ring_n = 0;   // заполнено слотов
    uint8_t _ring_i = 0;   // следующий слот для записи

    uint32_t _cal_sum = 0;
    uint8_t _cal_n = 0;
    uint16_t _cal_min = 0;
    uint16_t _cal_max = 0;
};

#endif
