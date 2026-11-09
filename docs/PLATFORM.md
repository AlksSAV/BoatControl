# PLATFORM

## 1. Решение

PlatformIO (6.2.0) + платформа pioarduino **55.03.312**, фреймворк **Arduino core 3.x**, плата `seeed_xiao_esp32c6`.

Проверено на этой машине:

- плата есть в PIO: `mcu esp32c6`, `variant XIAO_ESP32C6`, flash 4 МБ;
- pioarduino 55.03.312 установлена, официальная `espressif32` 6.10.0 — C6 **не** поддерживает;
- `toolchain-riscv32-esp`, `tool-cmake`, `tool-ninja` — есть;
- `idf.py` и `IDF_PATH` — отсутствуют (standalone ESP-IDF не настроен);
- `ARDUINO_USB_CDC_ON_BOOT=1` → отладка через USB, внешний UART не нужен.

Версию платформы фиксировать явно, не `@ latest`: API LEDC/ADC в 3.x меняется.

## 2. Маппинг (из `variants/XIAO_ESP32C6/pins_arduino.h`)

```
D0=0  D1=1  D2=2  D3=21 D4=22 D5=23 D6=16 D7=17 D8=19 D9=20 D10=18
A0=0  A1=1  A2=2
LED_BUILTIN=15   TX=16 RX=17   SDA=22 SCL=23
SS=21 MOSI=18 MISO=20 SCK=19   WIFI_ENABLE=3 WIFI_ANT_CONFIG=14
```

Следствия: LED на D6/D7 занимают UART0; PWM на D4/D5 занимают I2C. Ni один и не другой нам не нужны.

## 3. Доступные средства (проверено в SDK)

| Что | Факт |
|---|---|
| ADC | 12 бит, только 12 (`SOC_ADC_DIGI_MIN/MAX_BITWIDTH = 12`), аттенюатор `ADC_11db` |
| LEDC | есть, таймер 20 бит (`SOC_LEDC_TIMER_BIT_WIDTH = 20`), API `ledcAttach`, `ledcAttachChannel(pin,freq,res,channel)`, `ledcWrite(pin,duty)` |
| RMT | есть, TX-каналов **2** (`SOC_RMT_TX_CANDIDATES_PER_GROUP = 2`), API `rmtInit`, `rmtWrite`, `rmtTransmitCompleted` |
| Watchdog | `enableLoopWDT()`, `feedLoopWDT()` |

Для двух RC-линий хватает и LEDC, и ровно двух каналов RMT. Выбор — за кодом; при джиттере LEDC переходим на RMT (иных TX-каналов RMT на C6 нет).

## 4. Команды

```bash
pio run              # сборка (из корня проекта)
pio run -t upload    # прошивка
pio device monitor   # лог USB-CDC 115200
```

Не видит порт при прошивке — удерживать BOOT при подключении USB.

## 5. Структура

```
platformio.ini, include/config.h, src/   прошивка (в корне — стандарт PIO)
docs/       SPEC, HARDWARE, PLATFORM
hardware/   монтаж
tools/      разбор логов калибровки
```

Все числа — только в `include/config.h`.
