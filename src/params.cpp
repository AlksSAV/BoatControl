// BoatControl — runtime-параметры: дефолты, клампы, сквозные проверки (SPEC п.11).

#include "params.h"

Params g_params;

void params_default(Params* p) {
    p->thr_span_adc      = THR_SPAN_ADC;
    p->thr_deadband_adc  = THR_DEADBAND_ADC;
    p->thr_rate_up_pct   = THR_RATE_UP_PCT;
    p->thr_rate_down_pct = THR_RATE_DOWN_PCT;
    p->thr_zero_pass_ms  = THR_ZERO_PASS_MS;
    p->x_span_adc        = X_SPAN_ADC;
    p->x_deadband_adc    = X_DEADBAND_ADC;
    p->x_exponent        = X_EXPONENT;
    p->stb_slew_full_ms  = STB_SLEW_FULL_MS;
    p->stb_pulse_min     = STB_PULSE_MIN;
    p->stb_pulse_max     = STB_PULSE_MAX;
    p->cruise_min_pct    = CRUISE_MIN_PCT;
    p->cruise_step_pct   = CRUISE_STEP_PCT;
    p->cruise_edge_pct   = CRUISE_EDGE_PCT;
    p->cruise_ramp_ms    = CRUISE_RAMP_MS;
}

// Сквозные ограничения: карта делит на (span - deadband), поэтому мёртвая
// зона обязана быть строго меньше хода; импульсы сервы упорядочены;
// slew nonzero — иначе деление скорости хода сервы на ноль.
static bool params_cross_ok(const Params& p) {
    if (p.thr_deadband_adc >= p.thr_span_adc) return false;
    if (p.x_deadband_adc >= p.x_span_adc) return false;
    if (p.stb_pulse_min >= p.stb_pulse_max) return false;
    if (p.stb_pulse_min < RC_PULSE_NEUTRAL - RC_PULSE_SPAN) return false;
    if (p.stb_pulse_max > RC_PULSE_NEUTRAL + RC_PULSE_SPAN) return false;
    if (p.stb_slew_full_ms == 0) return false;
    if (p.cruise_ramp_ms == 0) return false;
    return true;
}

bool params_validate(const Params& p) {
    if (p.thr_span_adc < THR_SPAN_ADC_MIN || p.thr_span_adc > THR_SPAN_ADC_MAX) return false;
    if (p.thr_deadband_adc < THR_DB_ADC_MIN || p.thr_deadband_adc > THR_DB_ADC_MAX) return false;
    if (p.thr_rate_up_pct < THR_RATE_PCT_MIN || p.thr_rate_up_pct > THR_RATE_PCT_MAX) return false;
    if (p.thr_rate_down_pct < THR_RATE_PCT_MIN || p.thr_rate_down_pct > THR_RATE_PCT_MAX) return false;
    if (p.thr_zero_pass_ms > THR_ZERO_MS_MAX) return false;
    if (p.x_span_adc < X_SPAN_ADC_MIN || p.x_span_adc > X_SPAN_ADC_MAX) return false;
    if (p.x_deadband_adc < X_DB_ADC_MIN || p.x_deadband_adc > X_DB_ADC_MAX) return false;
    if (!(p.x_exponent >= X_EXPONENT_MIN && p.x_exponent <= X_EXPONENT_MAX)) return false;
    if (p.stb_slew_full_ms < STB_SLEW_MS_MIN || p.stb_slew_full_ms > STB_SLEW_MS_MAX) return false;
    if (p.stb_pulse_min < STB_PULSE_MIN_MIN || p.stb_pulse_min >= p.stb_pulse_max) return false;
    if (p.stb_pulse_max > STB_PULSE_MAX_MAX) return false;
    if (p.cruise_min_pct < CRUISE_MIN_PCT_MIN || p.cruise_min_pct > CRUISE_MIN_PCT_MAX) return false;
    if (p.cruise_step_pct < CRUISE_STEP_PCT_MIN || p.cruise_step_pct > CRUISE_STEP_PCT_MAX) return false;
    if (p.cruise_edge_pct < CRUISE_EDGE_PCT_MIN || p.cruise_edge_pct > CRUISE_EDGE_PCT_MAX) return false;
    if (p.cruise_ramp_ms < CRUISE_RAMP_MS_MIN || p.cruise_ramp_ms > CRUISE_RAMP_MS_MAX) return false;
    return params_cross_ok(p);
}

// Общий кламп: вне диапазона — false и поле не тронуто (веб получает 400,
// рабочий набор остаётся целым).
#define PARAMS_CLAMP(name, field, lo, hi)          \
    bool name(Params& p, decltype(p.field) v) {    \
        if (v < (lo) || v > (hi)) return false;    \
        p.field = v;                               \
        return true;                               \
    }

PARAMS_CLAMP(params_clamp_thr_span, thr_span_adc, THR_SPAN_ADC_MIN, THR_SPAN_ADC_MAX)
PARAMS_CLAMP(params_clamp_thr_deadband, thr_deadband_adc, THR_DB_ADC_MIN, THR_DB_ADC_MAX)
PARAMS_CLAMP(params_clamp_thr_rate_up, thr_rate_up_pct, THR_RATE_PCT_MIN, THR_RATE_PCT_MAX)
PARAMS_CLAMP(params_clamp_thr_rate_down, thr_rate_down_pct, THR_RATE_PCT_MIN, THR_RATE_PCT_MAX)
PARAMS_CLAMP(params_clamp_x_span, x_span_adc, X_SPAN_ADC_MIN, X_SPAN_ADC_MAX)
PARAMS_CLAMP(params_clamp_x_deadband, x_deadband_adc, X_DB_ADC_MIN, X_DB_ADC_MAX)
PARAMS_CLAMP(params_clamp_stb_slew_full, stb_slew_full_ms, STB_SLEW_MS_MIN, STB_SLEW_MS_MAX)
PARAMS_CLAMP(params_clamp_cruise_min_pct, cruise_min_pct, CRUISE_MIN_PCT_MIN, CRUISE_MIN_PCT_MAX)
PARAMS_CLAMP(params_clamp_cruise_step_pct, cruise_step_pct, CRUISE_STEP_PCT_MIN, CRUISE_STEP_PCT_MAX)
PARAMS_CLAMP(params_clamp_cruise_edge_pct, cruise_edge_pct, CRUISE_EDGE_PCT_MIN, CRUISE_EDGE_PCT_MAX)
PARAMS_CLAMP(params_clamp_cruise_ramp_ms, cruise_ramp_ms, CRUISE_RAMP_MS_MIN, CRUISE_RAMP_MS_MAX)

// Нижняя граница 0 осмысленна только для беззнакового поля, поэтому отдельно
// (в макете `v < 0` дало бы предупреждение).
bool params_clamp_thr_zero_pass(Params& p, uint16_t v) {
    if (v > THR_ZERO_MS_MAX) return false;
    p.thr_zero_pass_ms = v;
    return true;
}

bool params_clamp_x_exponent(Params& p, float v) {
    // Сравнение с NaN даёт false → NaN отклоняется.
    if (!(v >= X_EXPONENT_MIN && v <= X_EXPONENT_MAX)) return false;
    p.x_exponent = v;
    return true;
}

bool params_clamp_stb_pulse_min(Params& p, uint16_t v) {
    if (v < STB_PULSE_MIN_MIN || v >= p.stb_pulse_max) return false;
    p.stb_pulse_min = v;
    return true;
}

bool params_clamp_stb_pulse_max(Params& p, uint16_t v) {
    if (v > STB_PULSE_MAX_MAX || v <= p.stb_pulse_min) return false;
    p.stb_pulse_max = v;
    return true;
}
