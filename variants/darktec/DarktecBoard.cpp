#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "DarktecBoard.h"

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
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);

    _ina_ok = _ina.begin(TELEM_INA3221_ADDRESS, &Wire);
    if (_ina_ok) {
      _ina.setShuntResistance(DARKTEC_INA_CHARGE_CH, TELEM_INA3221_SHUNT_VALUE);
    }
}

void DarktecBoard::pollChargeSense() {
  uint32_t now = millis();
  if (_ina_last_ms != 0 && (now - _ina_last_ms) < 250) {
    return;
  }
  _ina_last_ms = now;

  if (!_ina_ok) {
    _ina_ok = _ina.begin(TELEM_INA3221_ADDRESS, &Wire);
    if (!_ina_ok) {
      return;
    }
    _ina.setShuntResistance(DARKTEC_INA_CHARGE_CH, TELEM_INA3221_SHUNT_VALUE);
  }

  float amps = _ina.getCurrentAmps(DARKTEC_INA_CHARGE_CH);
  if (isnan(amps)) {
    return;
  }
  float ma = amps * 1000.0f;
  if (_charging) {
    _charging = ma > DARKTEC_CHARGE_OFF_MA;
  } else {
    _charging = ma > DARKTEC_CHARGE_ON_MA;
  }
}

bool DarktecBoard::isCharging() {
  pollChargeSense();
  return _charging;
}

bool DarktecBoard::isExternalPowered() {
  // Pack charging is INA3221 CH2. USB VBUS is not pack charge, but KEEP_DISPLAY
  // and the stock charging icon also treat a USB cable as external power.
  return isCharging() || NRF52Board::isExternalPowered();
}
