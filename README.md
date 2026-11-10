# BoatControl

Катер: джойстик JH-D400X-R4 → XIAO ESP32-C6 → ESC (мотор) + серва (руль), кнопка круиза + зуммер.

## Схема

```
              XIAO ESP32-C6
   Стик X (руль) ──D0/GPIO0──┐            ┌──D4/GPIO22──→ сигнал ESC ──→ мотор
   Стик Y (газ)  ──D1/GPIO1──┤  ADC1      │
                             ├─ 50 Гц ────┼──D5/GPIO23──→ сигнал сервы ──→ руль
   Кнопка круиза ──D3/GPIO21─┤  LEDC      │
                             │            └──D8/GPIO19──→ зуммер (+ на пин, − на GND,
   LED готов  ←───D6/GPIO16──┤                              HIGH = писк)
   LED круиз  ←───D7/GPIO17──┘

   Джойстик JH-D400X-R4: 10 кОм, питание 3V3, ось Z и кнопка стика не используются.
   BEC/UBEC 5–6V ──→ серва и 5V платы; GND силовой и логики общий.
   Рубильник и предохранитель — в силовой цепи.
```

## Документация

- [docs/SPEC.md](docs/SPEC.md) — логика, карты, состояния, fail-safe
- [docs/HARDWARE.md](docs/HARDWARE.md) — пины, схема входа, питание, BOM
- [docs/PLATFORM.md](docs/PLATFORM.md) — PlatformIO + pioarduino 55.03.312, команды

## Плата

![XIAO ESP32-C6](docs/chip.jpeg)

## Сборка

```bash
pio run && pio run -t upload
```
