// BoatControl — кнопка круиза: debounce, short/long, залипание (SPEC п.6, п.7).

#ifndef BOAT_BUTTON_H
#define BOAT_BUTTON_H

#include <Arduino.h>
#include "config.h"

class Button {
  public:
    // Вызывать каждый такт с hal_button_pressed().
    void update(bool pressed, uint32_t now_ms);

    bool short_press() const { return _short; }   // фронт отпускания после короткого
    bool long_press() const { return _long; }     // достигнута длина длинного
    bool stuck() const { return _stuck; }         // удержание > BTN_STUCK_MS

  private:
    bool _stable = false;
    uint32_t _change_ms = 0;
    bool _short = false;
    bool _long = false;
    bool _long_sent = false;
    bool _stuck = false;
};

#endif
