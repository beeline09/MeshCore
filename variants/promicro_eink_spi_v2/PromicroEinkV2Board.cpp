#include <Arduino.h>
#include "PromicroEinkV2Board.h"

#ifdef NRF52_POWER_MANAGEMENT
const PowerMgtConfig power_config = {
  .lpcomp_ain_channel = PWRMGT_LPCOMP_AIN,
  .lpcomp_refsel      = PWRMGT_LPCOMP_REFSEL,
  .voltage_bootlock   = PWRMGT_VOLTAGE_BOOTLOCK,
  .lpcomp_low_refsel  = PWRMGT_LPCOMP_LOW_REFSEL,
};

void PromicroEinkV2Board::initiateShutdown(uint8_t reason) {
  digitalWrite(PIN_GPS_EN, LOW);

  if (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
      reason == SHUTDOWN_REASON_BOOT_PROTECT) {
    configureVoltageWake(power_config.lpcomp_ain_channel, power_config.lpcomp_refsel);
  }

  enterSystemOff(reason);
}
#endif

void PromicroEinkV2Board::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    // GPS power enable; LoRa is powered directly from 3.3V
    pinMode(PIN_GPS_EN, OUTPUT);
#ifdef NRF52_POWER_MANAGEMENT
    // Battery sense is on D17 -> P0.31 -> AIN7. We enforce the same Li-ion
    // startup threshold as the classic promicro before powering GPS.
    checkBootVoltage(&power_config);
    if (power_config.lpcomp_low_refsel) {
      configureLowVoltageAlert(power_config.lpcomp_ain_channel, power_config.lpcomp_low_refsel);
    }
#endif
    digitalWrite(PIN_GPS_EN, HIGH);
    delay(10);

    // Wire / I2C intentionally not initialized (no I2C devices in this variant)
}
