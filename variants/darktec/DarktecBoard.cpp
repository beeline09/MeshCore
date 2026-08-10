#include <Arduino.h>
#include <Wire.h>

#include "DarktecBoard.h"

#ifdef NRF52_POWER_MANAGEMENT
const PowerMgtConfig power_config = {
  .lpcomp_ain_channel = PWRMGT_LPCOMP_AIN,
  .lpcomp_refsel      = PWRMGT_LPCOMP_REFSEL,
  .voltage_bootlock   = PWRMGT_VOLTAGE_BOOTLOCK,
  .lpcomp_low_refsel  = PWRMGT_LPCOMP_LOW_REFSEL,
};

void DarktecBoard::initiateShutdown(uint8_t reason) {
  // LOW_VOLTAGE / BOOT_PROTECT: ADC sleep/wake, без SYSTEMOFF+LPCOMP.
  if (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
      reason == SHUTDOWN_REASON_BOOT_PROTECT) {
    darktec::waitForBatteryRecovery(*this, SX126X_POWER_EN);
    return;  // недостижимо
  }

  // Прочие причины (USER и т.п.) — прежнее поведение.
  digitalWrite(SX126X_POWER_EN, LOW);
  enterSystemOff(reason);
}
#endif

void DarktecBoard::powerOff() {
  // Companion AUTO_SHUTDOWN и ручной powerOff → тот же ADC recovery-цикл.
  darktec::waitForBatteryRecovery(*this, SX126X_POWER_EN);
}

void DarktecBoard::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
      Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
    #endif

    Wire.begin();

    pinMode(SX126X_POWER_EN, OUTPUT);
#ifdef NRF52_POWER_MANAGEMENT
    // Boot-lock по АЦП (химия): не SYSTEMOFF, а ожидание подъёма VBAT / USB.
    if (darktec::batteryBelowThreshold(*this, PWRMGT_VOLTAGE_BOOTLOCK)) {
      darktec::waitForBatteryRecovery(*this, SX126X_POWER_EN);
    }
    // LPCOMP runtime alert не включаем (lpcomp_low_refsel=0).
    (void)power_config;
#endif
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // дать sx1262 время на включение
}
