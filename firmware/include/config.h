// BoatControl — все параметры. Числа живут только здесь.
// Значения — из docs/SPEC.md (п. 3, 5-8); подтверждаются на стенде (SPEC п. 9).

#ifndef BOATCONFIG_H
#define BOATCONFIG_H

#include <Arduino.h>

// --- Пины (docs/HARDWARE.md п.2, проверено: variants/XIAO_ESP32C6) ---
constexpr uint8_t PIN_STICK_X    = 0;    // D0/A0, ADC1_CH0 — руль
constexpr uint8_t PIN_STICK_Y    = 1;    // D1/A1, ADC1_CH1 — газ
constexpr uint8_t PIN_BTN_CRUISE = 21;   // D3, pull-up, активный низ
constexpr uint8_t PIN_ESC        = 22;   // D4 — сигнал ESC
constexpr uint8_t PIN_SERVO      = 23;   // D5 — сигнал сервы
constexpr uint8_t PIN_LED_READY  = 16;   // D6
constexpr uint8_t PIN_LED_CRUISE = 17;   // D7
constexpr uint8_t PIN_LED_BUILT  = 15;   // бортовой LED, active-low

// --- RC-сигнал: 50 Гц, импульс 1000-2000 мкс (SPEC п.3) ---
constexpr uint32_t RC_FREQ_HZ       = 50;
constexpr uint32_t RC_PERIOD_US     = 20000;
constexpr uint16_t RC_PULSE_NEUTRAL = 1500;
constexpr uint16_t RC_PULSE_SPAN    = 500;  // нейтраль -> упор
constexpr uint8_t  LEDC_RESOLUTION  = 16;   // шаг ~0.3 мкс при 50 Гц

// --- Цикл и арминг (SPEC п.5) ---
constexpr uint16_t LOOP_PERIOD_MS       = 20;
constexpr uint16_t ARMING_DELAY_MS      = 1000;
constexpr uint8_t  ADC_SAMPLES_PER_LOOP = 16;

// --- АЦП (SPEC п.7) ---
constexpr uint16_t ADC_MAX_RAW   = 4095;  // 12 бит
constexpr uint16_t ADC_MID_RAW   = 2048;
constexpr uint16_t ADC_VALID_MIN = 100;   // ниже — обрыв
constexpr uint16_t ADC_VALID_MAX = 3995;  // выше — КЗ к питанию
constexpr uint16_t ADC_NOISE_MAX = 60;    // разброс средних за 1 с в READY

// --- Газ (SPEC п.3): нейтраль 1500=стоп, от себя=вперёд=1000 ---
constexpr uint16_t THR_DEADBAND_ADC  = 40;
constexpr uint16_t THR_SPAN_ADC      = 1700; // центр -> упор
constexpr uint8_t  THR_INVERT        = 0;    // 1 = инвертировать ось Y
constexpr uint8_t  THR_RATE_UP_PCT   = 8;    // набор, % за 100 мс
constexpr uint8_t  THR_RATE_DOWN_PCT = 25;   // сброс, % за 100 мс
constexpr uint16_t THR_ZERO_PASS_MS  = 250;  // пауза в нейтрали при смене знака

// --- Руль (SPEC п.3) ---
constexpr uint16_t X_DEADBAND_ADC = 30;
constexpr uint16_t X_SPAN_ADC     = 1700;
constexpr uint8_t  STB_INVERT     = 0;
constexpr float    X_EXPONENT     = 1.4f;
constexpr uint16_t STB_PULSE_MIN  = 1000;    // программный упор (SPEC п.3)
constexpr uint16_t STB_PULSE_MAX  = 2000;

// --- Кнопка и круиз (SPEC п.6) ---
constexpr uint16_t BTN_DEBOUNCE_MS  = 25;
constexpr uint16_t BTN_LONG_MS      = 2000;  // длинное = принудительный сброс
constexpr uint32_t BTN_STUCK_MS     = 10000; // залипание -> FAULT
constexpr uint8_t  CRUISE_MIN_PCT   = 12;    // минимум газа для включения
constexpr uint16_t CRUISE_RAMP_MS   = 800;   // возврат из круиза к стике

// --- Тарировка (SPEC п.5) ---
constexpr uint8_t  CAL_SAMPLES        = 50;  // средних за такты на ось
constexpr uint16_t CAL_MAX_OFFSET_ADC = 120; // допуск смещения нуля
constexpr uint16_t CAL_MAX_NOISE_ADC  = 25;  // разброс при покое

// --- Индикация (SPEC п.4) ---
constexpr uint16_t LED_BLINK_SLOW_MS = 600;  // CALIBRATE
constexpr uint16_t LED_BLINK_FAST_MS = 120;  // FAULT
constexpr uint16_t FAULT_CAL_BURST_MS = 2000; // период двойной вспышки

// --- Отладка: 1 = периодический лог в USB-CDC ---
#ifndef DEBUG_LOG
#define DEBUG_LOG 0
#endif
constexpr uint16_t DEBUG_LOG_PERIOD_MS = 200;

#endif // BOATCONFIG_H
