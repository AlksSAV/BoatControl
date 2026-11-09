# BoatControl

Катер: джойстик → XIAO ESP32-C6 → ESC (мотор) + серва (руль), кнопка круиза.

## Схема

```
   Стик X (руль) ──┐                    ┌── PWM 50Hz ──→ ESC ──→ мотор
   Стик Y (газ)  ──┤ ADC1  (D0, D1)     │
                   ├════════════════════╡ LEDC/RMT ─────┐
   Кнопка круиза ──┘ GPIO (D3)          │               │
                  XIAO ESP32-C6         ├── D4 ──→ сигнал ESC
   LED готов  ← D6                      │
   LED круиз  ← D7                      └── D5 ──→ сигнал сервы

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
cd firmware && pio run && pio run -t upload
```

Код прошивки не написан.
