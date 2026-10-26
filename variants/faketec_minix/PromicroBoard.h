#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

#define P_LORA_NSS 13 //P1.13 45
#define P_LORA_DIO_1 11 //P0.10 10
#define P_LORA_RESET 10 //P0.09 9
#define P_LORA_BUSY  16 //P0.29 29
#define P_LORA_MISO  15 //P0.02 2
#define P_LORA_SCLK  12 //P1.11 43
#define P_LORA_MOSI  14 //P1.15 47
#define SX126X_POWER_EN 22 //P0.13 13
#define SX126X_RXEN 2 //P0.17
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE (1.8f)

#define  PIN_VBAT_READ 17
#define  ADC_MULTIPLIER   (1.815f) // dependent on voltage divider resistors. TODO: more accurate battery tracking

#ifdef STATUS_LED_LORA_ACTIVITY
  // сколько светодиод горит после принятого пакета и после окончания передачи
  #ifndef STATUS_LED_RX_MILLIS
    #define STATUS_LED_RX_MILLIS 60
  #endif
  #ifndef STATUS_LED_TX_MILLIS
    #define STATUS_LED_TX_MILLIS 250
  #endif
#endif

class PromicroBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  float adc_mult = ADC_MULTIPLIER;

#ifdef STATUS_LED_LORA_ACTIVITY
  bool     _led_on = false;
  bool     _txing = false;
  uint32_t _led_off_at = 0;

  void setLed(bool on);
#endif

public:
  PromicroBoard() : NRF52Board("ProMicro_OTA") {}
  void begin();

#ifdef STATUS_LED_LORA_ACTIVITY
  // В покое погашен, короткая вспышка на принятый пакет, горит всю передачу.
  // Приём приходит из FaketecSX1262Wrapper, передача — из самого слоя радио.
  void onBeforeTransmit() override;
  void onAfterTransmit() override;
  void onLoRaPacketReceived();
  void updateStatusLed();
#endif

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (adc_mult * raw);
  }

  bool setAdcMultiplier(float multiplier) override {
    if (multiplier == 0.0f) {
      adc_mult = ADC_MULTIPLIER;}
    else {
      adc_mult = multiplier;
    }
    return true;
  }
  float getAdcMultiplier() const override {
    if (adc_mult == 0.0f) {
      return ADC_MULTIPLIER;
    } else {
      return adc_mult;
    }
  }

  const char* getManufacturerName() const override {
    return "ProMicro DIY";
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
