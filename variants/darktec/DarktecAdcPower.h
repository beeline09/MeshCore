#pragma once

/*
 * Darktec: hard cutoff / recovery по АЦП (режим DARKTEC_BATT_PROTECT_ADC).
 *
 * Питание MCU: батарея → внешний DC-DC buck-boost → 3V3.
 * АЦП: тот же пакет через делитель (D17 / AIN7) — единственный критерий wake.
 * USB VBUS MCU к питанию/зарядке/recovery не относится.
 *
 * SYSTEMOFF + LPCOMP здесь не подходит: остаёмся в System ON, гасим
 * SoftDevice/радио (иначе BLE → brownout → MCU обесточен и опрос мёртв),
 * периодически читаем делитель.
 *
 * Цикл: радио OFF → disable SoftDevice → sleep → АЦП ≥ wake → reset.
 */

#include <Arduino.h>
#include <helpers/NRF52Board.h>
#include <nrf_sdm.h>

#ifndef PWRMGT_VOLTAGE_WAKE
#define PWRMGT_VOLTAGE_WAKE 3600
#endif
#ifndef PWRMGT_VOLTAGE_BOOTLOCK
#define PWRMGT_VOLTAGE_BOOTLOCK 3200
#endif
#ifndef PWRMGT_VOLTAGE_CRITICAL
#define PWRMGT_VOLTAGE_CRITICAL 3000
#endif

#ifndef DARKTEC_ADC_POWER_POLL_MS
#define DARKTEC_ADC_POWER_POLL_MS  1000
#endif

#ifndef DARKTEC_ADC_WAKE_STABLE_COUNT
#define DARKTEC_ADC_WAKE_STABLE_COUNT  3
#endif

namespace darktec {

/** Снизить ток перед долгим ожиданием: иначе BLE/SD → brownout и цикл АЦП умирает. */
inline void prepareLowPowerWait(uint8_t radio_power_en_pin) {
  pinMode(radio_power_en_pin, OUTPUT);
  digitalWrite(radio_power_en_pin, LOW);

#if defined(PIN_VBAT_READ)
  pinMode(PIN_VBAT_READ, INPUT);
#elif defined(BATTERY_PIN)
  pinMode(BATTERY_PIN, INPUT);
#endif

  Serial.flush();

  uint8_t sd_enabled = 0;
  (void)sd_softdevice_is_enabled(&sd_enabled);
  if (sd_enabled) {
    // После disable() нельзя вызывать sd_* SVC — будет hardfault.
    (void)sd_softdevice_disable();
  }
}

/**
 * Блокирующий цикл ожидания восстановления пакета по АЦП.
 * radio_power_en_pin — SX126X_POWER_EN (выкл. на время ожидания).
 * Не возвращается: при recovery — NVIC_SystemReset().
 */
inline void waitForBatteryRecovery(mesh::MainBoard& board, uint8_t radio_power_en_pin) {
  prepareLowPowerWait(radio_power_en_pin);

  MESH_DEBUG_PRINTLN("DARKTEC: ADC power wait (bootlock=%u critical=%u wake=%u)",
      (unsigned)PWRMGT_VOLTAGE_BOOTLOCK,
      (unsigned)PWRMGT_VOLTAGE_CRITICAL,
      (unsigned)PWRMGT_VOLTAGE_WAKE);

  uint8_t wake_hits = 0;

  for (;;) {
    delay(20);
    analogReadResolution(12);
    uint16_t mv = board.getBattMilliVolts();

    if (mv > 1000 && mv >= (uint16_t)PWRMGT_VOLTAGE_WAKE) {
      if (++wake_hits >= DARKTEC_ADC_WAKE_STABLE_COUNT) {
        MESH_DEBUG_PRINTLN("DARKTEC: VBAT %u mV >= wake — reset", (unsigned)mv);
        NVIC_SystemReset();
      }
    } else {
      wake_hits = 0;
    }

    delay(DARKTEC_ADC_POWER_POLL_MS);
  }
}

/** true, если напряжение пакета ниже порога (только АЦП, без USB). */
inline bool batteryBelowThreshold(mesh::MainBoard& board, uint16_t threshold_mv) {
  uint16_t mv = board.getBattMilliVolts();
  return (mv > 1000 && mv < threshold_mv);
}

}  // namespace darktec
