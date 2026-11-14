// BoatControl — runtime-параметры (SPEC п.11): пороги, правятся из сервисного
// веба и живут в NVS. Дефолт каждого — соответствующая константа config.h,
// поэтому без записи NVS поведение ровно как до веба (режим v1.0).
//
// Чистая логика: файл не тянет Arduino-зависимостей сверх mock-заглушки и
// линкуется в нативные тесты. NVS-хранение — в params_store.cpp.

#ifndef BOAT_PARAMS_H
#define BOAT_PARAMS_H

#include <Arduino.h>
#include "config.h"

struct Params {
    // Газ (SPEC п.3)
    uint16_t thr_span_adc;     // дефолт THR_SPAN_ADC
    uint16_t thr_deadband_adc; // THR_DEADBAND_ADC
    uint8_t  thr_rate_up_pct;  // THR_RATE_UP_PCT
    uint8_t  thr_rate_down_pct;// THR_RATE_DOWN_PCT
    uint16_t thr_zero_pass_ms; // THR_ZERO_PASS_MS
    // Руль (SPEC п.3)
    uint16_t x_span_adc;       // X_SPAN_ADC
    uint16_t x_deadband_adc;   // X_DEADBAND_ADC
    float    x_exponent;       // X_EXPONENT
    uint16_t stb_slew_full_ms; // STB_SLEW_FULL_MS
    uint16_t stb_pulse_min;    // STB_PULSE_MIN
    uint16_t stb_pulse_max;    // STB_PULSE_MAX
    // Круиз (SPEC п.6)
    uint8_t  cruise_min_pct;   // CRUISE_MIN_PCT
    uint8_t  cruise_step_pct;  // CRUISE_STEP_PCT
    uint8_t  cruise_edge_pct;  // CRUISE_EDGE_PCT
    uint16_t cruise_ramp_ms;   // CRUISE_RAMP_MS
};

// Значения по умолчанию = константы config.h.
void params_default(Params* p);

// Проверка и ограничение всех полей. Возвращает false, если хоть одно
// значение вне диапазона (оно НЕ меняется — вызывающий решает, отклонять
// всю пачку или брать дефолт).
bool params_validate(const Params& p);

// Поле в границы; false = пришло значение вне диапазона.
bool params_clamp_thr_span(Params& p, uint16_t v);
bool params_clamp_thr_deadband(Params& p, uint16_t v);
bool params_clamp_thr_rate_up(Params& p, uint8_t v);
bool params_clamp_thr_rate_down(Params& p, uint8_t v);
bool params_clamp_thr_zero_pass(Params& p, uint16_t v);
bool params_clamp_x_span(Params& p, uint16_t v);
bool params_clamp_x_deadband(Params& p, uint16_t v);
bool params_clamp_x_exponent(Params& p, float v);
bool params_clamp_stb_slew_full(Params& p, uint16_t v);
bool params_clamp_stb_pulse_min(Params& p, uint16_t v);
bool params_clamp_stb_pulse_max(Params& p, uint16_t v);
bool params_clamp_cruise_min_pct(Params& p, uint8_t v);
bool params_clamp_cruise_step_pct(Params& p, uint8_t v);
bool params_clamp_cruise_edge_pct(Params& p, uint8_t v);
bool params_clamp_cruise_ramp_ms(Params& p, uint16_t v);

// Глобальный рабочий набор параметров. В setup() загружается из NVS
// (params_load), правится только из loop-потока через очередь команд веба.
extern Params g_params;

// --- NVS (SPEC п.11). Реализация: params_store.cpp, не линкуется в native ---
// true = в NVS есть корректная запись, значения проверены и скопированы в *p.
bool params_load(Params* p);
// true = запись сохранена и подтверждена обратным чтением.
bool params_store(const Params& p);
// Сброс к дефолтам config.h (удаление записи NVS).
bool params_erase();

#endif // BOAT_PARAMS_H
