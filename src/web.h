// BoatControl — сервисный веб-интерфейс (SPEC п.11).
//
// SoftAP поднимается в setup() и живёт только в сервисном окне: если за
// WEB_AP_TIMEOUT_MS никто не ассоциировался, радио гаснет до перезапитки.
// Выход из READY (DRIVE/CRUISE/FAULT) закрывает окно немедленно.
//
// Весь код работает в контексте loop(): WebServer обслуживается одним вызовом
// web_update() за такт, блокирующих сетевых ожиданий нет. Команды из браузера
// не меняют состояние машины напрямую — только кладутся в очередь, которую
// разбирает loop().

#ifndef BOAT_WEB_H
#define BOAT_WEB_H

#include <Arduino.h>
#include "config.h"
#include "params.h"

#if WEB_ENABLE

// Снимок для телеметрии: заполняется в loop() перед web_update().
struct WebStatus {
    const char* state;      // "BOOT"/"READY"/...
    uint8_t     state_id;
    uint16_t    esc_us;     // фактический импульс ESC
    uint16_t    servo_us;   // фактический импульс сервы
    uint16_t    ax_raw;     // сырое среднее оси X
    uint16_t    ay_raw;
    int32_t     x_off;      // смещение от нуля, единицы эталонного хода
    int32_t     y_off;
    uint16_t    center_x;   // тарированный центр (АЦП)
    uint16_t    center_y;
    uint16_t    span_x;     // калиброванный полуход (0 = нет калибровки)
    uint16_t    span_y;
    uint16_t    cruise_us;  // уставка круиза
    bool        cal_pending;  // запрошена ручная калибровка
};

enum class WebCmd : uint8_t { NONE, CALIBRATE, CLOSE };

// Поднять SoftAP и веб-сервер. Вызывать из setup() после hal_init().
void web_init();

// Один такт обслуживания: окно AP, таймауты, handleClient. Не блокирует.
void web_update(const WebStatus& st, uint32_t now_ms);

// Закрыть сервисный режим до следующей перезапитки (уход в DRIVE/CRUISE/FAULT).
void web_close();

// true = AP ещё активна (окно не истекло, режим не закрывали).
bool web_active();

// Команды из браузера: loop() забирает их по одной за такт.
bool web_take_cmd(WebCmd* out);

#endif  // WEB_ENABLE
#endif  // BOAT_WEB_H
