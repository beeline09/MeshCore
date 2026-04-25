#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

#ifndef ADC_MULTIPLIER
  #define ADC_MULTIPLIER 5.42
#endif

class HeltecT190Board : public ESP32Board {

public:
  RefCountedDigitalPin periph_power;

  HeltecT190Board() : periph_power(PIN_VEXT_EN,PIN_VEXT_EN_ACTIVE) { }

  float getAdcMultiplier() const override {
    return (adc_mult == 0.0f) ? ADC_MULTIPLIER : adc_mult;
  }
  void begin();
  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1);
  void powerOff() override;
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override ;

};
