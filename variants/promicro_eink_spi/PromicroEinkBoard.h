#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

// LoRa SX1262 / LLCC68 — shared SPI bus (D12/D14/D15)
#define P_LORA_NSS    13  // D13 = P1.13
#define P_LORA_DIO_1  11  // D11 = P0.10
#define P_LORA_RESET  10  // D10 = P0.09
#define P_LORA_BUSY   16  // D16 = P0.29
#define P_LORA_MISO   15  // D15 = P0.02
#define P_LORA_SCLK   12  // D12 = P1.11
#define P_LORA_MOSI   14  // D14 = P1.15

#define SX126X_POWER_EN       21  // D21 = P0.13 — EXT_VCC: enables 3.3V for radio and display
#define SX126X_RXEN           2   // D2  = P0.17 — external RX-enable
#define SX126X_TXEN           RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE  (1.8f)  // TCXO voltage on DIO3 (RA-62 and E22/E22P)

// E-ink display — shared SPI bus (CS separate from radio)
#define PIN_DISPLAY_CS    20  // D20 = P1.07
#define PIN_DISPLAY_DC    19  // D19 = P1.02
#define PIN_DISPLAY_RST   18  // D18 = P1.01
#define PIN_DISPLAY_BUSY  9   // D9  = P1.06

// 5-button D-pad
#define KEY_LEFT    3   // D3  = P0.20
#define KEY_RIGHT   4   // D4  = P0.22
#define KEY_UP      5   // D5  = P0.24
#define KEY_SELECT  6   // D6  = P1.00  (also PIN_BUTTON1 in variant.h)
#define KEY_DOWN    17  // D17 = P0.31  (shared with battery ADC — no MomentaryButton)

// Battery ADC on D17 = P0.31 (AIN7).
// Requires external voltage divider: VBAT → 100kΩ → D17 → 100kΩ → GND.
// KEY_DOWN shares this pin; if divider is connected, ADC reads VBAT/2.
#define PIN_VBAT_READ   17   // D17 = P0.31 = AIN7
#define ADC_MULTIPLIER  (1.73f)

class PromicroEinkBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  float adc_mult = ADC_MULTIPLIER;

public:
  PromicroEinkBoard() : NRF52Board("ProMicroEink_OTA") {}
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
    return "ProMicro WeAct Eink DIY";
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
