#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>

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
//#define KEY_DOWN    7   // D7  = P0.11  (free GPIO, no conflicts)

// Battery ADC on D17 = P0.31 (AIN7).
// Voltage divider: VBAT → 100kΩ → D17 → 150kΩ → GND  (ratio = 150/250 = 0.60)
// ADC_MULTIPLIER = Vref / (2^12 * ratio) * 1000 = 3600 / (4095 * 0.6) = 1.465
// KEY_DOWN shares this pin; button shorts D17 to GND when pressed.
#define PIN_VBAT_READ   17   // D17 = P0.31 = AIN7
#define ADC_MULTIPLIER  (1.47f)

class PromicroEinkBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  uint16_t battery_prev_state = 0;
  uint32_t battery_last_read_time = 0;
  float adc_mult = ADC_MULTIPLIER;

public:
  PromicroEinkBoard() : NRF52Board("ProMicroEink_OTA") {}
  void begin();

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    // Проверяем, прошло ли 10 минут с последнего чтения (300000 мс = 5 минут)
    uint32_t current_time = millis();
    if (current_time - battery_last_read_time < 300000 && battery_last_read_time != 0) {
      return battery_prev_state;
    }
    
    analogReadResolution(12);
    uint32_t raw = 0;
    uint32_t sample = 0;

    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      sample = analogRead(PIN_VBAT_READ);
      if(sample < 250) return battery_prev_state;
      raw += sample;
    }

    raw = raw / BATTERY_SAMPLES;       
    
    battery_last_read_time = current_time;
    battery_prev_state = (uint16_t)(adc_mult * raw);
    
    return battery_prev_state;
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

  bool isExternalPowered() override {
    uint8_t sd_enabled = 0;
    sd_softdevice_is_enabled(&sd_enabled);

    uint32_t usb_status = 0;
    if (sd_enabled) {
      sd_power_usbregstatus_get(&usb_status);
    } else {
      usb_status = NRF_POWER->USBREGSTATUS;
    }
    return (usb_status & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
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
