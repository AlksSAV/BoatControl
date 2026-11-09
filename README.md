# BoatControl

Управление катером: мотор через ESC и рулевая серва (PWM 50 Гц, 1000–2000 мкс),
аналоговый джойстик с самовозвратом (газ + руль) и кнопка круиза.
Плата — Seeed Studio XIAO ESP32-C6.

## Документация

| Файл | Содержание |
|---|---|
| [docs/SPEC.md](docs/SPEC.md) | Логика: сигнальные карты, состояния, круиз, fail-safe, все пороги |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Распиновка, схема входа стика, питание, BOM |
| [docs/PLATFORM.md](docs/PLATFORM.md) | Выбранный тулчейн и команды сборки |

## Структура

```
docs/        спецификация
firmware/    прошивка (PlatformIO, пока только platformio.ini)
```

## Сборка

```bash
cd firmware
pio run
pio run -t upload
pio device monitor
```

Код прошивки ещё не написан.
