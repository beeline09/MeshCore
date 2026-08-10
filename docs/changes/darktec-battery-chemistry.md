# Darktec — химия батареи и ADC sleep/wake

Форк аппаратного профиля ProMicro nRF52840 (`variants/darktec`) с выбором
химии батареи. Основной `src/helpers/NRF52Board.cpp` **не изменяется**.

## Делитель батареи / АЦП

Делитель: **100к / 100к** (верхнее / нижнее плечо, коэффициент 2.0) на `D17` → AIN7.

`ADC_MULTIPLIER = 1.750f` в `DarktecBoard.h` — калибровка по мультиметру.

## Hard cutoff без LPCOMP / SYSTEMOFF

Зарядка батареи — **своим зарядником на плате**, не через USB fakeTec/ProMicro.
USB VBUS MCU к штатной зарядке не относится.

На этой плате `SYSTEMOFF` + LPCOMP wake после разряда ненадёжен при подъёме
VBAT (boost/DCDC). Darktec использует цикл в `DarktecAdcPower.h`:

1. Радио off (`SX126X_POWER_EN = LOW`)
2. Sleep ~1 с, замер АЦП
3. Если `mv >= PWRMGT_VOLTAGE_WAKE` (пакет подтянул внешний зарядник) → reset
4. Иначе если USB VBUS на MCU (запасной путь) → reset
5. Иначе снова sleep

Срабатывает при:

- boot, если `mv < PWRMGT_VOLTAGE_BOOTLOCK` и нет USB на MCU
- `initiateShutdown(LOW_VOLTAGE|BOOT_PROTECT)`
- `powerOff()` (в т.ч. companion `AUTO_SHUTDOWN_MILLIVOLTS`)

LPCOMP отключён (`PWRMGT_LPCOMP_*=0`).

## Конфигурация

```ini
-D BATTERY_CHEMISTRY=BATTERY_CHEM_LIION
-D BATTERY_CELLS=1
-D TELEM_INA3221_SHUNT_VALUE=0.050
```

| Химия | empty/critical | bootlock | wake | UI full |
|-------|----------------|----------|------|---------|
| Li-ion 1S | 3000 | 3200 | 3600 | 4200 |
| LiFePO4 1S | 2500 | 2700 | 3100 | 3650 |
| LTO 1S | 1800 | 2000 | 2200 | 2500 |
| LTO 2S | ×2 | ×2 | ×2 | ×2 |

## Окружения сборки

`Darktec_repeater`, `Darktec_companion_radio_ble`, и остальные аналоги ProMicro.
