// BoatControl — кнопка: debounce, короткое/длинное нажатие, залипание (SPEC п.6, п.7).

#include "button.h"

void Button::update(bool pressed, uint32_t now_ms) {
    _short = false;
    _long = false;

    // смена уровня -> старт debounce
    if (pressed != _stable) {
        if (now_ms - _change_ms >= BTN_DEBOUNCE_MS) {
            _stable = pressed;
            _change_ms = now_ms;
            if (pressed) {
                _long_sent = false;
                _stuck = false;
            } else {
                // отпустили: если длинное ещё не сработало — это короткое
                if (!_long_sent) _short = true;
            }
        }
    } else if (_stable) {
        // удерживается
        uint32_t held = now_ms - _change_ms;
        if (!_long_sent && held >= BTN_LONG_MS) {
            _long = true;
            _long_sent = true;
        }
        if (held >= BTN_STUCK_MS) _stuck = true;
    } else {
        _change_ms = now_ms;  // в покое — держать отсчёт актуальным
    }
}
