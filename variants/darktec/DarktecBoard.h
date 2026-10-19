#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>
#include <Adafruit_INA3221.h>

#define PIN_VBAT_READ 17
// Battery divider 100k / 100k. Default ADC_MULTIPLIER is in variant.h (1.750f).
#ifndef ADC_MULTIPLIER
#define ADC_MULTIPLIER   (1.750f)
#endif

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
// Plain charge-current hysteresis is replaced by the DarktecSocModel charge
// automat (spec 0001 FR-34): TPS54302 has no charge termination, so "full"
// is detected by the firmware, not by the hardware.

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
  uint32_t _vbus_gone_ms = 0;  // timestamp of VBUS absence with an active charge status
  uint32_t _tx_hold_until = 0;
  uint32_t _rx_hold_until = 0;

  void pollChargeSense();

public:
  DarktecBoard() : NRF52Board("Darktec_OTA") {}
  void begin();
  bool isCharging();
  // spec 0001 FR-34: automat in the Full phase (plug instead of bolt).
  bool isChargeFull();
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

  // Raw ADC read without side effects (used by the SOC model tick, spec 0001).
  uint16_t readBattMvRaw() {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (uint16_t)(adc_mult * raw);
  }

  // Raw ADC read without side effects (used by the SOC model tick, spec 0001)
  // and by the INA sensors page (spec 0002).
  bool readInaChannel(int ch, float* bus_v, float* ma) {
    if (!_ina_ok || ch < 0 || ch > 2) return false;
    *bus_v = _ina.getBusVoltage((uint8_t)ch);
    float acc = 0;
    for (int i = 0; i < 3; i++) acc += _ina.getCurrentAmps((uint8_t)ch);
    *ma = acc / 3.0f * 1000.0f;
    return true;
  }

  uint16_t getBattMilliVolts() override {
    return readBattMvRaw();
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
