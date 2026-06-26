#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

// Battery voltage-divider ratio. Ported from Meshtastic (ADC_MULTIPLIER 2.08).
#ifndef BATTERY_ADC_MULTIPLIER
  #define BATTERY_ADC_MULTIPLIER 2.0f
#endif

class BuildLabScoutBoard : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override { return "BuildLab Scout"; }
};
