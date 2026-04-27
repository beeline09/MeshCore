#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

// LoRa SX1262 (E22P) — same SPI pins as promicro variant, custom bus via SPI.setPins()
#define P_LORA_NSS    13  // D13 = P1.13
#define P_LORA_DIO_1  11  // D11 = P0.10
#define P_LORA_RESET  10  // D10 = P0.09
#define P_LORA_BUSY   16  // D16 = P0.29
#define P_LORA_MISO   15  // D15 = P0.02
#define P_LORA_SCLK   12  // D12 = P1.11
#define P_LORA_MOSI   14  // D14 = P1.15

// E22P: LoRa powered directly from 3.3V — no software power switch
// SX126X_POWER_EN intentionally not defined
#define SX126X_RXEN               RADIOLIB_NC
#define SX126X_TXEN               RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH  false
#define SX126X_DIO3_TCXO_VOLTAGE  (1.8f)

// E-ink display (SPI1: SCK=D18/P1.01, MOSI=D20/P1.07, MISO=D19/NC)
#define PIN_DISPLAY_CS    5   // D5  = P0.24
#define PIN_DISPLAY_DC    7   // D7  = P0.11
#define PIN_DISPLAY_RST   9   // D9  = P1.06
#define PIN_DISPLAY_BUSY  8   // D8  = P1.04

// GPS power enable (exclusive — LoRa on direct 3.3V)
#define PIN_GPS_EN  21   // D21 = P0.13

// Battery ADC: D17 = P0.31 (AIN7), 100k/100k voltage divider
#define PIN_VBAT_READ   17
#define ADC_MULTIPLIER  (1.815f)

class PromicroEinkV2Board : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  float adc_mult = ADC_MULTIPLIER;

public:
  PromicroEinkV2Board() : NRF52Board("ProMicroEinkV2_OTA") {}
  void begin();

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
    adc_mult = (multiplier == 0.0f) ? ADC_MULTIPLIER : multiplier;
    return true;
  }

  float getAdcMultiplier() const override {
    return (adc_mult == 0.0f) ? ADC_MULTIPLIER : adc_mult;
  }

  const char* getManufacturerName() const override {
    return "ProMicro EinkV2 DIY";
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

  void powerOff() override {
    sd_power_system_off();
  }
};
