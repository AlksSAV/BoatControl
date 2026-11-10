#include "hal.h"

// LEDC 50 Гц / 16 бит: кадр 20000 мкс делится на 65536 шага,
// шаг ~0.305 мкс. dutycycle = us * 65536 / 20000.
static inline uint32_t us_to_duty(uint16_t us) {
    return (uint32_t)us * ((1UL << LEDC_RESOLUTION) - 1UL) / RC_PERIOD_US;
}

void hal_init() {
    pinMode(PIN_BTN_CRUISE, INPUT_PULLUP);
    pinMode(PIN_LED_READY, OUTPUT);
    pinMode(PIN_LED_CRUISE, OUTPUT);
    pinMode(PIN_LED_BUILT, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED_READY, LOW);
    digitalWrite(PIN_LED_CRUISE, LOW);
    digitalWrite(PIN_LED_BUILT, HIGH);  // active-low: выкл
    digitalWrite(PIN_BUZZER, LOW);

    // Обе RC-линии сразу в нейтраль: выход никогда не в Hi-Z (SPEC п.7).
    ledcAttach(PIN_ESC, RC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttach(PIN_SERVO, RC_FREQ_HZ, LEDC_RESOLUTION);
    ledcWrite(PIN_ESC, us_to_duty(RC_PULSE_NEUTRAL));
    ledcWrite(PIN_SERVO, us_to_duty(RC_PULSE_NEUTRAL));

    analogReadResolution(12);
}

void hal_esc_pulse(uint16_t us) {
    ledcWrite(PIN_ESC, us_to_duty(us));
}

void hal_servo_pulse(uint16_t us) {
    ledcWrite(PIN_SERVO, us_to_duty(us));
}

uint16_t hal_adc(uint8_t pin) {
    return analogRead(pin);
}

bool hal_button_pressed() {
    return digitalRead(PIN_BTN_CRUISE) == LOW;
}

void hal_led_ready(bool on) {
    digitalWrite(PIN_LED_READY, on ? HIGH : LOW);
}

void hal_led_cruise(bool on) {
    digitalWrite(PIN_LED_CRUISE, on ? HIGH : LOW);
}

void hal_led_built(bool on) {
    digitalWrite(PIN_LED_BUILT, on ? LOW : HIGH);  // active-low
}

void hal_buzzer(bool on) {
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

void hal_wdt_start() {
    // Логика обязана позвать hal_wdt_feed() каждый такт (SPEC п.7).
    enableLoopWDT();
}

void hal_wdt_feed() {
    feedLoopWDT();
}
