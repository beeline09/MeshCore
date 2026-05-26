# ProMicro NRF52840 + WeAct Epaper 2.13" + EBYTE E22/E22P или RA-62

Вариант `promicro_eink_spi` — companion radio / repeater на базе:

- **MCU:** ProMicro NRF52840 (совместимый с Adafruit Feather nRF52840)
- **Радио:** EBYTE E22-868M30S / E22-900M30S / E22P-868M30S / E22P-900M30S (SX1262) **или** Heltec HT-RA62 (SX1262)
- **Дисплей:** WeAct Studio Epaper 2.13" (250×122, GxEPD2_213_B74)
- **UI:** 5-кнопочный D-pad (LEFT / RIGHT / UP / DOWN / SELECT)
- **SPI:** единая шина для радио и дисплея (D12/D14/D15), CS раздельные

> Радио и дисплей делят одну SPI-шину. Конфликтов нет — CS-пины разные (D13 для радио, D20 для дисплея), и шина инициализируется один раз в `radio.std_init()`.

---

## Распиновка

```
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
| nRF52 | Pin   | LORA (E22/E22P/RA-62) | E-ink (shared SPI)        | Input                                 | Output                |
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
| GND   | GND   |                       | GND                       |                                       |                       |
| P0.06 | D1    |                       |                           |                                       |                       |
| P0.08 | D0    |                       |                           |                                       |                       |
| GND   | GND   |                       | GND                       |                                       |                       |
| GND   | GND   |                       | GND                       |                                       |                       |
| P0.17 | D2    | RXEN                  |                           |                                       |                       |
| P0.20 | D3    |                       |                           | KEY LEFT                              |                       |
| P0.22 | D4    |                       |                           | KEY RIGHT                             |                       |
| P0.24 | D5    |                       |                           | KEY UP                                |                       |
| P1.00 | D6    |                       |                           | KEY SELECT (INPUT_PULLUP)             |                       |
| P0.11 | D7    |                       |                           |                                       |                       |
| P1.04 | D8    |                       |                           |                                       |                       |
| P1.06 | D9    |                       | Epaper BUSY               |                                       |                       |
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
| P0.09 | D10   | NRESET                |                           |                                       |                       |
| P0.10 | D11   | DIO1 (IRQ)            |                           |                                       |                       |
| P1.11 | D12   | SCK                   | SCK (shared)              |                                       |                       |
| P1.13 | D13   | NSS (CS)              |                           |                                       |                       |
| P1.15 | D14   | MOSI                  | MOSI (shared)             |                                       |                       |
| P0.02 | D15   | MISO                  |                           |                                       |                       |
| P0.29 | D16   | BUSY                  |                           |                                       |                       |
| P0.31 | D17   |                       |                           | KEY DOWN / ADC (AIN7 — VBAT divider)  |                       |
| 3V3   | P0.13 | EXT_VCC               | 3V3                       |                                       | EXT_VCC               |
| RST   | RST   |                       |                           |                                       |                       |
| GND   | GND   |                       | GND                       |                                       |                       |
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
| P1.01 | D18   |                       | Epaper RST                |                                       |                       |
| P1.02 | D19   |                       | Epaper D/C                |                                       |                       |
| P1.07 | D20   |                       | Epaper CS                 |                                       |                       |
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
| P0.15 | D22   |                       |                           |                                       | PIN_LED               |
+-------+-------+-----------------------+---------------------------+---------------------------------------+-----------------------+
```

**Примечания:**
- **D17 (KEY DOWN / ADC)** — двойная функция. С делителем 100кОм/100кОм: ADC читает VBAT/2. Без делителя: цифровая кнопка с INPUT_PULLUP. В текущей реализации D17 инициализируется как `INPUT` (для ADC); `MomentaryButton` для KEY DOWN не создаётся.
- **D0, D1** — свободны.
- **D7, D8** — свободны.
- **EXT_VCC (D21 = P0.13)** — управляет питанием радио и дисплея через транзистор/MOSFET.

---

## Схема подключения

### Радио EBYTE E22/E22P/RA-62 ↔ ProMicro (SPI, D12/D14/D15)

```
ProMicro D12 (P1.11) ──── Radio SCK
ProMicro D14 (P1.15) ──── Radio MOSI
ProMicro D15 (P0.02) ──── Radio MISO
ProMicro D16 (P0.29) ──── Radio BUSY
ProMicro D13 (P1.13) ──── Radio NSS (CS)
ProMicro D10 (P0.09) ──── Radio NRESET
ProMicro D11 (P0.10) ──── Radio DIO1
ProMicro D2  (P0.17) ──── Radio RXEN
ProMicro D21 (P0.13) ──── EXT_VCC (через транзистор/MOSFET → Radio VCC)
GND ────────────────────── Radio GND
```

### Дисплей WeAct Epaper 2.13" ↔ ProMicro (shared SPI)

```
ProMicro D12 (P1.11) ──── Epaper SCK   (та же шина, что и радио)
ProMicro D14 (P1.15) ──── Epaper MOSI  (та же шина, что и радио)
ProMicro D20 (P1.07) ──── Epaper CS
ProMicro D19 (P1.02) ──── Epaper D/C
ProMicro D18 (P1.01) ──── Epaper RST
ProMicro D9  (P1.06) ──── Epaper BUSY
3.3V ───────────────────── Epaper VCC
GND ────────────────────── Epaper GND
```

> Epaper MISO не подключается — дисплей работает только на запись.

### Кнопки D-pad (5-way joystick module)

Подключение 5-кнопочного джойстик-модуля (типа "5-way navigation switch / tactile joystick"):

```
Joystick UP   ──── ProMicro D5  (P0.24)  [= back_btn в UI]
Joystick DOWN ──── ProMicro D17 (P0.31)  [INPUT / ADC]
Joystick LEFT ──── ProMicro D3  (P0.20)  [= joystick_left в UI]
Joystick RIGHT──── ProMicro D4  (P0.22)  [= joystick_right в UI]
Joystick MID  ──── ProMicro D6  (P1.00)  [= user_btn / SELECT в UI]
Joystick COM  ──── GND
```

Все сигнальные пины подтянуты внутри MCU (INPUT_PULLUP) — внешние резисторы не нужны (кроме D17 с делителем VBAT).

### Батарея — делитель напряжения (на D17)

```
VBAT ──── 100 кОм ──── D17 (P0.31) ──── 100 кОм ──── GND
```

`ADC_MULTIPLIER = 1.73f`. Калибровка: `AT+ADCMULT=<value>` или напрямую в коде.

---

## Пресеты радио

| Вариант | Freq (MHz) | BW (kHz) | SF | CR | Примечание |
|---------|-----------|----------|----|----|------------|
| HT-RA62 (SX1262) | 869.075 | 62.5 | 8 | 8 | Прошит в конфиге RA62-окружений |
| E22 / E22P (SX1262) | — | — | — | — | По умолчанию (задаётся через AT) |

---

## Архитектура SPI

| Шина | Устройства | SCK | MOSI | MISO | CS |
|------|-----------|-----|------|------|----|
| SPI  | Радио + Дисплей | D12 | D14 | D15 | D13 (радио), D20 (дисплей) |

Шина инициализируется один раз в `radio.std_init(&SPI)`. `GxEPDDisplay::begin()` переключается на неё через `display.epd2.selectSPI(SPI, ...)` без повторного вызова `SPI.begin()` (флаг компиляции `EINK_SHARED_SPI`).

---

## Маппинг кнопок в UI

| Физическая кнопка | Переменная в target.cpp | Назначение в UI |
|-------------------|------------------------|-----------------|
| KEY SELECT (D6)   | `user_btn`             | Подтвердить / Select |
| KEY LEFT (D3)     | `joystick_left`        | Навигация влево / предыдущий |
| KEY RIGHT (D4)    | `joystick_right`       | Навигация вправо / следующий |
| KEY UP (D5)       | `back_btn`             | Назад / Back |
| KEY DOWN (D17)    | —                      | Зарезервирован (ADC) |

---

## Сборка прошивки

### Окружения

| Окружение | Описание |
|-----------|----------|
| `ProMicro_WeAct_RA62_companion_radio_ble`  | Companion radio, HT-RA62 (SX1262), BLE |
| `ProMicro_WeAct_RA62_companion_radio_usb`  | Companion radio, HT-RA62 (SX1262), USB |
| `ProMicro_WeAct_RA62_repeater`             | Репитер, HT-RA62 (SX1262) |
| `ProMicro_WeAct_RA62_room_server`          | Room server, HT-RA62 (SX1262) |
| `ProMicro_WeAct_E22_companion_radio_ble`   | Companion radio, E22 M30S, BLE |
| `ProMicro_WeAct_E22_companion_radio_usb`   | Companion radio, E22 M30S, USB |
| `ProMicro_WeAct_E22_repeater`              | Репитер, E22 M30S |
| `ProMicro_WeAct_E22_room_server`           | Room server, E22 M30S |
| `ProMicro_WeAct_E22P_companion_radio_ble`  | Companion radio, E22P M30S (RX patch), BLE |
| `ProMicro_WeAct_E22P_companion_radio_usb`  | Companion radio, E22P M30S (RX patch), USB |
| `ProMicro_WeAct_E22P_repeater`             | Репитер, E22P M30S |
| `ProMicro_WeAct_E22P_room_server`          | Room server, E22P M30S |

### Команды

```bash
pio run -e ProMicro_WeAct_RA62_companion_radio_ble
pio run -e ProMicro_WeAct_RA62_companion_radio_ble --target upload
```
