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
  digitalWrite(SX126X_POWER_EN, LOW);

  if (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
      reason == SHUTDOWN_REASON_BOOT_PROTECT) {
    configureVoltageWake(power_config.lpcomp_ain_channel, power_config.lpcomp_refsel);
  }

  enterSystemOff(reason);
}
#endif

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
    // Датчик батареи: D17 → P0.31 → AIN7.
    // Boot-lock (voltage_bootlock) и LPCOMP DOWN (lpcomp_low_refsel) для Darktec
    // выключены (=0): иначе после cutoff плата не оживает при подъёме VBAT
    // (boost/DCDC), только от USB. Шкала % берётся из BATT_MIN/MAX.
    checkBootVoltage(&power_config);
    if (power_config.lpcomp_low_refsel) {
      configureLowVoltageAlert(power_config.lpcomp_ain_channel, power_config.lpcomp_low_refsel);
    }
#endif
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // дать sx1262 время на включение
}
