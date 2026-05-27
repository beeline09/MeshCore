#include <Arduino.h>
#include <SPI.h>
#include "PromicroEinkBoard.h"

void PromicroEinkBoard::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    // D17 = P0.31: battery ADC only (PIN_VBAT_READ). KEY_DOWN moved to D7.
    // nRF52 SAADC reads AIN7 directly; no pinMode() needed for ADC-only pins.

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    // EXT_VCC: enables 3.3V power rail for radio and display
    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // give radio time to power up

    // Drive radio NSS HIGH before display init to prevent radio receiving
    // garbage SPI bytes while display is initialised on the shared SPI bus.
    pinMode(P_LORA_NSS, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);

    // Pre-initialise shared SPI bus so display.begin() works before radio_init().
    // radio.std_init() will call SPI.setPins()+SPI.begin() again with the same
    // pins — that is harmless.
    SPI.setPins(P_LORA_MISO, P_LORA_SCLK, P_LORA_MOSI);
    SPI.begin();
}
