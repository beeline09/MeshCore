#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>
#include <Adafruit_INA3221.h>

#define PIN_VBAT_READ 17
// Battery divider 100k / 100k (nominal 2.0). Calibrated with a multimeter.
#define ADC_MULTIPLIER   (1.750f)

// INA3221: pack charging on channel 2 (Adafruit 0-based: CH1=0, CH2=1, CH3=2).
#ifndef TELEM_INA3221_ADDRESS
#define TELEM_INA3221_ADDRESS 0x42
#endif
#ifndef TELEM_INA3221_SHUNT_VALUE
#define TELEM_INA3221_SHUNT_VALUE 0.050f
#endif
#ifndef DARKTEC_INA_CHARGE_CH
#define DARKTEC_INA_CHARGE_CH 1
#endif
#ifndef DARKTEC_CHARGE_ON_MA
#define DARKTEC_CHARGE_ON_MA  120.0f
#endif
#ifndef DARKTEC_CHARGE_OFF_MA
#define DARKTEC_CHARGE_OFF_MA 100.0f
#endif

class DarktecBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  float adc_mult = ADC_MULTIPLIER;
  Adafruit_INA3221 _ina;
  bool _ina_ok = false;
  bool _charging = false;
  bool _txing = false;
  bool _rxing = false;
  uint32_t _ina_last_ms = 0;
  uint32_t _tx_hold_until = 0;
  uint32_t _rx_hold_until = 0;

  void pollChargeSense();

public:
  DarktecBoard() : NRF52Board("Darktec_OTA") {}
  void begin();
  bool isCharging();
  bool isExternalPowered() override;
  bool isLoRaActivity() const {
    return _txing || _rxing
        || (int32_t)(millis() - _tx_hold_until) < 0
        || (int32_t)(millis() - _rx_hold_until) < 0;
  }
  void setLoRaReceiving(bool v) { _rxing = v; }
  void onLoRaPacketReceived() { _rx_hold_until = millis() + 80; }
  void onBeforeTransmit() override { _txing = true; }
  void onAfterTransmit() override {
    _txing = false;
    _tx_hold_until = millis() + 80;
  }

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (uint16_t)(adc_mult * raw);
  }

  bool setAdcMultiplier(float multiplier) override {
    if (multiplier == 0.0f) {
      adc_mult = ADC_MULTIPLIER;
    } else {
      adc_mult = multiplier;
    }
    return true;
  }
  float getAdcMultiplier() const override {
    if (adc_mult == 0.0f) {
      return ADC_MULTIPLIER;
    }
    return adc_mult;
  }

  const char* getManufacturerName() const override {
    return "Darktec";
  }

  int buttonStateChanged() {
    #ifdef BUTTON_PIN
      uint8_t v = digitalRead(BUTTON_PIN);
      if (v != btn_prev_state) {
        btn_prev_state = v;
        return (v == LOW) ? 1 : -1;
      }
    #endif
      return 0;
  }
};
