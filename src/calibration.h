// BoatControl — хранение калибровки в NVS (SPEC п.5.5).
// Храним только полуходы осей: ноль тарируется авто-тарировкой каждый старт
// (SPEC п.5, он дрейфует от температуры), поэтому центр в NVS не нужен.
// Формат записи: заголовок (magic+version) + значения + контрольная сумма FNV-1a.

#ifndef BOAT_CALIBRATION_H
#define BOAT_CALIBRATION_H

#include <Arduino.h>
#include "config.h"

struct CalData {
    uint16_t span_x;
    uint16_t span_y;
};

// true = в NVS есть корректная запись; значения скопированы в *out.
bool cal_load(CalData* out);

// true = запись сохранена и подтверждена обратным чтением.
bool cal_store(const CalData& data);

// Сброс записи (для стенда: вернуть работу по константам из config.h).
bool cal_erase();

#endif // BOAT_CALIBRATION_H
