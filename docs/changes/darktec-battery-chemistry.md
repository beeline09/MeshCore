# Darktec — химия батареи и режим защиты питания

Форк аппаратного профиля ProMicro nRF52840 (`variants/darktec`) с выбором
химии батареи. Основной `src/helpers/NRF52Board.cpp` **не изменяется**.

## Делитель батареи / АЦП

Делитель: **100к / 100к** (верхнее / нижнее плечо, коэффициент 2.0) на `D17` → AIN7.

`ADC_MULTIPLIER = 1.750f` в `DarktecBoard.h` — калибровка по мультиметру.

Зарядка батареи — **своим зарядником на плате**, не через USB fakeTec/ProMicro.
USB VBUS MCU к штатной зарядке не относится (только запасной wake в режиме ADC).

## Режим защиты: `DARKTEC_BATT_PROTECT`

В `variants/darktec/platformio.ini` (секция `[Darktec]`):

```ini
; Без hard cutoff (только шкала % по химии)
-D DARKTEC_BATT_PROTECT=DARKTEC_BATT_PROTECT_OFF

; Sleep/wake по АЦП (по умолчанию)
-D DARKTEC_BATT_PROTECT=DARKTEC_BATT_PROTECT_ADC
```

| Режим | Поведение |
|-------|-----------|
| `DARKTEC_BATT_PROTECT_OFF` | Нет boot-lock / ADC wait / companion auto-shutdown. `powerOff()` → классический `SYSTEMOFF`. Химия только для % на UI. |
| `DARKTEC_BATT_PROTECT_ADC` | Hard cutoff без LPCOMP: радио off → sleep → опрос АЦП → при `VBAT ≥ wake` (внешний зарядник) или USB MCU → reset. |

LPCOMP на Darktec **не используется** в обоих режимах (`PWRMGT_LPCOMP_*=0`): связка SYSTEMOFF+LPCOMP на boost не поднимает плату от VBAT.

### Режим ADC — цикл (`DarktecAdcPower.h`)

1. Радио off (`SX126X_POWER_EN = LOW`)
2. Sleep ~1 с, замер АЦП
3. Если `mv >= PWRMGT_VOLTAGE_WAKE` (пакет подтянул внешний зарядник) → reset
4. Иначе если USB VBUS на MCU (запасной путь) → reset
5. Иначе снова sleep

Срабатывает при:

- boot, если `mv < PWRMGT_VOLTAGE_BOOTLOCK` и нет USB на MCU
- `initiateShutdown(LOW_VOLTAGE|BOOT_PROTECT)`
- `powerOff()` (в т.ч. companion `AUTO_SHUTDOWN_MILLIVOLTS`)

## Химия батареи

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

Пороги bootlock/wake/critical применяются только при `DARKTEC_BATT_PROTECT_ADC`.
`BATT_MIN/MAX` (шкала %) — всегда.

## Окружения сборки

`Darktec_repeater`, `Darktec_companion_radio_ble`, и остальные аналоги ProMicro.
