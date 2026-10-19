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
#define SX126X_POWER_EN 21 //P0.13 13

// Тип радиомодуля (spec 0001 FR-2, D-6): 0 HT-RA62, 1 E22-900M30S (дефолт),
// 2 E22P-868M30S, 3 E22-900M33S. У E22P пин 6 модуля — это EN (общий
// включатель RF-тракта, активный высокий): RadioLib вёл его как RXEN и на
// передаче опускал в 0, гася тракт целиком. При модуле 2 пин выводится
// из-под библиотеки (SX126X_RXEN не задаётся) и держится высоким вручную
// в DarktecBoard::begin(). Остальные модули — текущая разводка.
#ifndef DARKTEC_RADIO_MODULE
#define DARKTEC_RADIO_MODULE 1
#endif
#if DARKTEC_RADIO_MODULE < 0 || DARKTEC_RADIO_MODULE > 3
#error "DARKTEC_RADIO_MODULE должен быть 0 (HT-RA62), 1 (E22-900M30S), 2 (E22P-868M30S) или 3 (E22-900M33S)"
#endif
#if DARKTEC_RADIO_MODULE == 2
// EN E22P: P0.17 (Arduino 2) — вручную из begin(), RadioLib не трогает.
#define DARKTEC_RADIO_EN_PIN 2
#else
#define SX126X_RXEN 2 //P0.17
#endif
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE (1.8f)

#define  PIN_VBAT_READ 17
// Делитель батареи: 100к / 100к. Дефолт ADC_MULTIPLIER — в variant.h (1.750f).
#ifndef ADC_MULTIPLIER
#define  ADC_MULTIPLIER   (1.750f)
#endif

#include "DarktecAdcPower.h"
#include <Adafruit_INA3221.h>

// INA3221: зарядка пакета на 2-м канале (Adafruit 0-based: CH1=0, CH2=1, CH3=2).
#ifndef TELEM_INA3221_ADDRESS
#define TELEM_INA3221_ADDRESS 0x42
#endif
#ifndef TELEM_INA3221_SHUNT_VALUE
#define TELEM_INA3221_SHUNT_VALUE 0.050f
#endif
#ifndef DARKTEC_INA_CHARGE_CH
#define DARKTEC_INA_CHARGE_CH 1
#endif
// Пороги детекции заряда заменены автоматом DarktecSocModel (FR-34).


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
#ifdef NRF52_POWER_MANAGEMENT
  void initiateShutdown(uint8_t reason) override;
#endif

public:
  DarktecBoard() : NRF52Board("Darktec_OTA") {}
  void begin();
  bool isCharging();
  // spec 0001 FR-34: автомат в фазе Full (для вилки вместо молнии).
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
    _tx_hold_until = millis() + 80;  // короткий хвост, чтобы вспышка была видна на OLED
  }

  #define BATTERY_SAMPLES 8

  // Сырое чтение АЦП без побочных эффектов (защита юга, SOC-модель).
  uint16_t readBattMvRaw() {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (uint16_t)(adc_mult * raw);
  }

  uint16_t getBattMilliVolts() override {
    return readBattMvRaw();
  }

  // Спецификация 0002 FR-11: прямое чтение канала INA3221 для страницы
  // датчиков (0 = SP, 1 = DC, 2 = Bat). false — INA недоступна.
  bool readInaChannel(int ch, float* bus_v, float* ma) {
    if (!_ina_ok || ch < 0 || ch > 2) return false;
    *bus_v = _ina.getBusVoltage((uint8_t)ch);
    float acc = 0;
    for (int i = 0; i < 3; i++) acc += _ina.getCurrentAmps((uint8_t)ch);
    *ma = acc / 3.0f * 1000.0f;
    return true;
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

  // Hibernate / CLI: SYSTEMOFF. Защита батареи — ADC-wait в initiateShutdown.
  void powerOff() override;
};
