# Darktec — химия батареи и управление питанием

Форк аппаратного профиля ProMicro nRF52840 (`variants/darktec`) с выбором
химии батареи. Основной `src/helpers/NRF52Board.cpp` **не изменяется**.

## Делитель батареи / АЦП

Делитель: **100к / 100к** (верхнее / нижнее плечо, коэффициент 2.0) на `D17` → AIN7.

`ADC_MULTIPLIER = 1.750f` в `DarktecBoard.h` — калибровка по мультиметру
(в рантайме можно подкрутить через `set adc_multiplier`).

## Cutoff / recovery

Жёсткий `SYSTEMOFF` через boot-lock и LPCOMP DOWN на этой плате **отключён**
(`PWRMGT_VOLTAGE_BOOTLOCK=0`, `PWRMGT_LPCOMP_LOW_REFSEL=0`).

Причина: после cutoff при подъёме напряжения пакета boost/DCDC ProMicro не
возвращал плату к жизни — оживал только USB. Без аппаратного lock плата
оживает при подъёме VBAT, как оригинальная прошивка без порогов.

Химия влияет на **шкалу %** (`BATT_MIN/MAX_MILLIVOLTS`), не на hard cutoff.

## Конфигурация

В `variants/darktec/platformio.ini` (секция `[Darktec]`):

```ini
-D BATTERY_CHEMISTRY=BATTERY_CHEM_LIION
-D BATTERY_CELLS=1
-D TELEM_INA3221_SHUNT_VALUE=0.050
```

| `BATTERY_CHEMISTRY` | `BATTERY_CELLS` | empty→full (мВ, 1S) |
|---------------------|------------------|---------------------|
| `BATTERY_CHEM_LIION` | 1 | 3000→4200 |
| `BATTERY_CHEM_LIFEPO4` | 1 | 2500→3650 |
| `BATTERY_CHEM_LTO` | 1 или 2 | 1800→2500 (×cells) |

Неверные комбинации дают `#error` на этапе препроцессора.

## Окружения сборки

`Darktec_repeater`, `Darktec_companion_radio_ble`, `Darktec_companion_radio_usb`
и остальные аналоги ProMicro.
