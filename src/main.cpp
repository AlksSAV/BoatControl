// BoatControl — главный цикл и машина состояний (SPEC п.4-7).
//
// BOOT -> CALIBRATE -> READY -> DRIVE <-> CRUISE, любой сбой -> FAULT.
// Цикл строго 50 Гц (LOOP_PERIOD_MS), в конце такта — кормёжка WDT.

#include <Arduino.h>
#include "config.h"
#include "hal.h"
#include "stick.h"
#include "throttle.h"
#include "steering.h"
#include "button.h"

enum class State : uint8_t { BOOT, CALIBRATE, READY, DRIVE, CRUISE, FAULT };

static State g_state = State::BOOT;

static Stick stick_x(PIN_STICK_X);
static Stick stick_y(PIN_STICK_Y);
static Throttle throttle;
static Button button;

static uint32_t t_boot_ms = 0;        // момент включения
static uint32_t t_last_ms = 0;        // прошлый такт
static uint16_t esc_pulse = RC_PULSE_NEUTRAL;
static uint16_t servo_pulse = RC_PULSE_NEUTRAL;

// Круиз (SPEC п.6)
static uint16_t cruise_pulse = RC_PULSE_NEUTRAL;  // зафиксированное значение
static bool cruise_exiting = false;               // плавный возврат к стике

static void enter(State s) {
    g_state = s;
    if (s == State::BOOT || s == State::CALIBRATE) {
        throttle.force(RC_PULSE_NEUTRAL);
        cruise_exiting = false;
    }
    if (s == State::CALIBRATE) {
        stick_x.cal_reset();
        stick_y.cal_reset();
    }
    if (s == State::FAULT) {
        throttle.force(RC_PULSE_NEUTRAL);
        cruise_exiting = false;
        cruise_pulse = RC_PULSE_NEUTRAL;
    }
}

// Индикация (SPEC п.4, п.5)
static void update_leds(uint32_t now) {
    static uint32_t t_blink = 0;
    static bool blink_on = false;
    if (now - t_blink >= LED_BLINK_SLOW_MS) {
        t_blink = now;
        blink_on = !blink_on;
    }

    bool fast_on = ((now / LED_BLINK_FAST_MS) % 2) == 0;

    switch (g_state) {
        case State::BOOT:
            hal_led_ready(false);
            hal_led_built(false);
            break;
        case State::CALIBRATE:
            hal_led_ready(false);
            hal_led_built(blink_on);
            break;
        case State::READY:
        case State::DRIVE:
            hal_led_ready(true);
            hal_led_built(true);
            break;
        case State::CRUISE:
            hal_led_ready(true);
            hal_led_built(true);
            break;
        case State::FAULT:
            hal_led_ready(fast_on);
            hal_led_built(fast_on);
            break;
    }
    hal_led_cruise(g_state == State::CRUISE);
}

// Газ по стике Y с учётом инверсии (SPEC п.3)
static uint16_t throttle_target() {
    int32_t off = stick_y.offset();
    if (THR_INVERT) off = -off;
    // По SPEC: стик «от себя» = вперёд. Считаем offset>0 = «от себя»;
    // если на стенде наоборот — выставим THR_INVERT=1.
    return Throttle::map_pulse(off);
}

static uint16_t servo_target() {
    int32_t off = stick_x.offset();
    if (STB_INVERT) off = -off;
    return Steering::map_pulse(off);
}

static void handle_button(uint32_t now) {
    // SPEC п.6, п.7
    if (button.stuck()) {
        enter(State::FAULT);
        return;
    }
    if (button.long_press()) {
        // принудительный сброс круиза
        if (g_state == State::CRUISE) {
            cruise_exiting = true;
            enter(State::DRIVE);
        }
        return;
    }
    if (button.short_press()) {
        if (g_state == State::DRIVE) {
            uint16_t out = throttle.out();
            int32_t off = stick_y.offset();
            if (THR_INVERT) off = -off;
            // включение: только вперёд и выше порога (SPEC п.6)
            bool forward = (out < RC_PULSE_NEUTRAL);
            if (forward && Throttle::percent(out) >= CRUISE_MIN_PCT) {
                cruise_pulse = out;   // фиксируем ФАКТИЧЕСКИЙ выход
                cruise_exiting = false;
                enter(State::CRUISE);
            }
        } else if (g_state == State::CRUISE) {
            cruise_exiting = true;    // плавный возврат к стике
            enter(State::DRIVE);
        }
    }
}

void loop() {
    uint32_t now = millis();
    if (now - t_last_ms < LOOP_PERIOD_MS) return;
    uint16_t dt = (uint16_t)(now - t_last_ms);
    t_last_ms = now;

    button.update(hal_button_pressed(), now);

    switch (g_state) {
        case State::BOOT:
            if (now - t_boot_ms >= ARMING_DELAY_MS) enter(State::CALIBRATE);
            break;

        case State::CALIBRATE: {
            stick_x.cal_add();
            stick_y.cal_add();
            if (stick_x.cal_n_done() >= CAL_SAMPLES &&
                stick_y.cal_n_done() >= CAL_SAMPLES) {
                if (stick_x.cal_finish() && stick_y.cal_finish()) {
                    enter(State::READY);
                } else {
                    enter(State::FAULT);  // ошибка калибровки
                }
            }
            break;
        }

        case State::READY: {
            stick_x.update();
            stick_y.update();
            if (stick_x.broken() || stick_y.broken()) { enter(State::FAULT); break; }
            // Шум АЦП в покое -> перекалибровка (SPEC п.7)
            if (stick_y.noisy() || stick_x.noisy()) {
                enter(State::CALIBRATE);
                break;
            }
            uint16_t tgt = throttle_target();
            if (tgt != RC_PULSE_NEUTRAL) enter(State::DRIVE);
            break;
        }

        case State::DRIVE: {
            stick_x.update();
            stick_y.update();
            if (stick_x.broken() || stick_y.broken()) { enter(State::FAULT); break; }

            uint16_t tgt = throttle_target();
            uint16_t cap = 0;
            if (cruise_exiting && tgt != throttle.out()) {
                // плавный возврат из круиза за CRUISE_RAMP_MS (SPEC п.6)
                cap = RC_PULSE_SPAN * dt / CRUISE_RAMP_MS;
                if (cap == 0) cap = 1;
            } else {
                cruise_exiting = false;
            }
            throttle.slew_to(tgt, dt, cap);
            if (tgt == RC_PULSE_NEUTRAL && throttle.out() == RC_PULSE_NEUTRAL) {
                cruise_exiting = false;
                enter(State::READY);
            }
            break;
        }

        case State::CRUISE: {
            stick_x.update();          // руль работает (SPEC п.6)
            stick_y.update();          // читаем, но игнорируем
            if (stick_x.broken()) { enter(State::FAULT); break; }
            // обрыв оси Y в круизе газ не роняет (SPEC п.6/7);
            // недостоверность зафиксированного значения невозможна программно —
            // значение валидно на момент фиксации
            throttle.force(cruise_pulse);
            break;
        }

        case State::FAULT:
            // только перезапитание (SPEC п.4). Выходы в нейтрали.
            throttle.force(RC_PULSE_NEUTRAL);
            break;
    }

    handle_button(now);

    // Выходы руля: нейтраль в BOOT/CALIBRATE/FAULT, иначе по стике (SPEC п.4)
    if (g_state == State::BOOT || g_state == State::CALIBRATE ||
        g_state == State::FAULT) {
        servo_pulse = RC_PULSE_NEUTRAL;
    } else {
        servo_pulse = servo_target();
    }
    esc_pulse = throttle.out();

    hal_esc_pulse(esc_pulse);
    hal_servo_pulse(servo_pulse);
    update_leds(now);

#if DEBUG_LOG
    static uint32_t t_log = 0;
    if (now - t_log >= DEBUG_LOG_PERIOD_MS) {
        t_log = now;
        Serial.printf("st=%u esc=%u stb=%u x=%d y=%d cx=%u cy=%u\n",
                      (unsigned)g_state, esc_pulse, servo_pulse,
                      (int)stick_x.offset(), (int)stick_y.offset(),
                      stick_x.center(), stick_y.center());
    }
#endif

    hal_wdt_feed();
}

void setup() {
#if DEBUG_LOG
    Serial.begin(115200);
#endif
    hal_init();
    hal_wdt_start();
    t_boot_ms = millis();
    t_last_ms = millis();
    enter(State::BOOT);
}
