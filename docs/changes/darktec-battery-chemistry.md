# Darktec — химия батареи и управление питанием

Форк аппаратного профиля ProMicro nRF52840 (`variants/darktec`) с выбором
химии батареи. Основной `src/helpers/NRF52Board.cpp` **не изменяется**.

## Делитель батареи / АЦП

Делитель: **100к / 100к** (верхнее / нижнее плечо, коэффициент 2.0) на `D17` → AIN7.

`ADC_MULTIPLIER = 1.750f` в `DarktecBoard.h` — калибровка по мультиметру
(в рантайме можно подкрутить через `set adc_multiplier`).

Пороги химии заданы в мВ пакета и через множитель уже согласованы с делителем
для пути АЦП. LPCOMP REFSEL для не-Li-ion желательно проверить на плате.

## Конфигурация

В `variants/darktec/platformio.ini` (секция `[Darktec]`):

```ini
-D BATTERY_CHEMISTRY=BATTERY_CHEM_LIION
-D BATTERY_CELLS=1
-D TELEM_INA3221_SHUNT_VALUE=0.050
```

| `BATTERY_CHEMISTRY` | `BATTERY_CELLS` | Примечание |
|---------------------|------------------|------------|
| `BATTERY_CHEM_LIION` | 1 | По умолчанию |
| `BATTERY_CHEM_LIFEPO4` | 1 | Только 1S |
| `BATTERY_CHEM_LTO` | 1 или 2 | 2S full = 5000 мВ (лимит входа 5 В) |

Неверные комбинации дают `#error` на этапе препроцессора.

Пороги (на ячейку × число ячеек) в `battery_chemistry.h` → `BATT_MIN/MAX_MILLIVOLTS`,
`PWRMGT_VOLTAGE_BOOTLOCK`, `PWRMGT_VOLTAGE_CRITICAL`, LPCOMP REFSEL.

## Порог critical без правок NRF52Board

Базовый `lpcompDownHandler()` сравнивает с хардкодом `3000` мВ.
Darktec этот файл не трогает: `DarktecBattPowerHelper.h` в контексте IRQ
подстраивает значение из `getBattMilliVolts()`, чтобы сохранить семантику
`PWRMGT_VOLTAGE_CRITICAL`. В обычном коде (телеметрия / UI) отдаются
реальные мВ.

## Окружения сборки

`Darktec_repeater`, `Darktec_companion_radio_ble`, `Darktec_companion_radio_usb`
и остальные аналоги ProMicro.
