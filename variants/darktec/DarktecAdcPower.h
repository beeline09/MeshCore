#pragma once

/*
 * Darktec: hard cutoff / recovery по АЦП (режим DARKTEC_BATT_PROTECT_ADC).
 *
 * Зарядка пакета — своим зарядником на плате, НЕ через USB fakeTec/ProMicro.
 * Основной путь пробуждения: подъём VBAT выше PWRMGT_VOLTAGE_WAKE (АЦП).
 * USB VBUS — только запасной путь (отладка / кабель в MCU).
 *
 * Цикл: радио OFF → sleep → опрос АЦП → wake или USB → NVIC_SystemReset().
 * При DARKTEC_BATT_PROTECT_OFF этот путь не вызывается.
 */

#include <Arduino.h>
#include <helpers/NRF52Board.h>

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

namespace darktec {

/**
 * Блокирующий цикл ожидания восстановления питания.
 * radio_power_en_pin — пин SX126X_POWER_EN (выключается на время ожидания).
 * Не возвращается: при recovery делает NVIC_SystemReset().
 */
inline void waitForBatteryRecovery(mesh::MainBoard& board, uint8_t radio_power_en_pin) {
  pinMode(radio_power_en_pin, OUTPUT);
  digitalWrite(radio_power_en_pin, LOW);

  MESH_DEBUG_PRINTLN("DARKTEC: ADC power wait (bootlock=%u critical=%u wake=%u)",
      (unsigned)PWRMGT_VOLTAGE_BOOTLOCK,
      (unsigned)PWRMGT_VOLTAGE_CRITICAL,
      (unsigned)PWRMGT_VOLTAGE_WAKE);

  for (;;) {
    // Главное: пакет заряжается внешним зарядником → растёт VBAT на делителе.
    uint16_t mv = board.getBattMilliVolts();
    if (mv > 1000 && mv >= (uint16_t)PWRMGT_VOLTAGE_WAKE) {
      MESH_DEBUG_PRINTLN("DARKTEC: VBAT %u mV >= wake — reset", (unsigned)mv);
      NVIC_SystemReset();
    }

    // Запасной путь: питание MCU по USB (не штатная зарядка батареи).
    if (board.isExternalPowered()) {
      MESH_DEBUG_PRINTLN("DARKTEC: USB VBUS present — reset");
      NVIC_SystemReset();
    }

    delay(DARKTEC_ADC_POWER_POLL_MS);
  }
}

/** true, если нужно уйти в wait (нет USB и напряжение ниже порога). */
inline bool batteryBelowThreshold(mesh::MainBoard& board, uint16_t threshold_mv) {
  if (board.isExternalPowered()) return false;
  uint16_t mv = board.getBattMilliVolts();
  return (mv > 1000 && mv < threshold_mv);
}

}  // namespace darktec
