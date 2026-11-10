// BoatControl — тонкий слой железа. Логика отсюда видит только эти функции.
// Реализация: LEDC (50 Гц, 16 бит) + analogRead + GPIO + loop WDT.

#ifndef BOAT_HAL_H
#define BOAT_HAL_H

#include <Arduino.h>
#include "config.h"

void hal_init();

// RC-линии: импульс в мкс (1000..2000). Вызывать каждый такт.
void hal_esc_pulse(uint16_t us);
void hal_servo_pulse(uint16_t us);

// АЦП: сырое 12-битное значение 0..4095
uint16_t hal_adc(uint8_t pin);

// Кнопка: true = нажата (уровень низкий, pull-up включён в hal_init)
bool hal_button_pressed();

// Светодиоды: true = горит. Бортовой LED active-low учтён внутри.
void hal_led_ready(bool on);
void hal_led_cruise(bool on);
void hal_led_built(bool on);

// Активный зуммер: true = писк (HIGH на выводе).
void hal_buzzer(bool on);

// Сторожевой таймер. Кормить раз в такт, иначе сброс платы.
void hal_wdt_start();
void hal_wdt_feed();

#endif
