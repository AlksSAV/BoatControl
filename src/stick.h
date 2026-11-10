// BoatControl — ось джойстика: oversampling, тарировка нуля, контроль валидности.
// Смещения возвращаются в НОРМАЛИЗОВАННЫХ единицах: масштаб к эталонному ходу
// (_ref_span) применяется по откалиброванному _span, чтобы порог жеста
// «до упора» и карты работали одинаково на любом экземпляре стика.

#ifndef BOAT_STICK_H
#define BOAT_STICK_H

#include <Arduino.h>
#include "config.h"

class Stick {
  public:
    // ref_span — эталонный ход оси из config.h (к нему нормализуем).
    Stick(uint8_t pin, uint16_t ref_span) : _pin(pin), _ref_span(ref_span) {}

    // Один такт: ADC_SAMPLES_PER_LOOP отсчётов -> среднее. Возвращает среднее.
    uint16_t update();

    // Смещение от тарированного нуля в единицах эталонного хода, со знаком.
    // Инверсию оси применяет вызывающий.
    int32_t offset() const;

    // Среднее вне ADC_VALID_MIN..MAX (SPEC п.7).
    bool broken() const { return _mean < ADC_VALID_MIN || _mean > ADC_VALID_MAX; }

    // Разброс средних за ~1 с выше ADC_NOISE_MAX (SPEC п.7).
    bool noisy() const;

    // Авто-тарировка нуля по шагам цикла (не блокирующая): reset -> add() -> finish().
    void cal_reset();
    void cal_add();
    bool cal_finish();  // true = допуск пройден
    uint8_t cal_n_done() const { return _cal_n; }

    // Ручная калибровка min/max (SPEC п.5.5): reset -> add() каждый такт -> finish().
    void man_reset();
    void man_add();
    bool man_finish();  // true = размах достаточен
    bool calibrated() const { return _calibrated; }

    uint16_t center() const { return _center; }
    uint16_t span() const { return _span; }
    void apply(uint16_t center, uint16_t span) {  // загрузка из NVS
        _center = center;
        _span = span;
        _calibrated = true;
    }
    uint16_t last() const { return _mean; }

  private:
    static constexpr uint8_t WINDOW = 1000 / LOOP_PERIOD_MS;  // ~1 с средних

    uint8_t _pin;
    uint16_t _ref_span;
    uint16_t _mean = ADC_MID_RAW;
    uint16_t _center = ADC_MID_RAW;
    uint16_t _span = 0;              // 0 = калибровки хода нет
    bool _calibrated = false;

    uint16_t _ring[WINDOW] = {0};
    uint8_t _ring_n = 0;   // заполнено слотов
    uint8_t _ring_i = 0;   // следующий слот для записи

    uint32_t _cal_sum = 0;
    uint8_t _cal_n = 0;
    uint16_t _cal_min = 0;
    uint16_t _cal_max = 0;

    uint16_t _man_min = 0;
    uint16_t _man_max = 0;
    bool _man_seen = false;
};

#endif
