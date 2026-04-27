#include <Arduino.h>
#include "PromicroEinkV2Board.h"

void PromicroEinkV2Board::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    // GPS power enable; LoRa is powered directly from 3.3V
    pinMode(PIN_GPS_EN, OUTPUT);
    digitalWrite(PIN_GPS_EN, HIGH);
    delay(10);

    // Wire / I2C intentionally not initialized (no I2C devices in this variant)
}
