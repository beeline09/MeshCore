#include <Arduino.h>
#include <Wire.h>

#include "PromicroBoard.h"
#include "target.h"

void PromicroBoard::begin() {    
    NRF52Board::begin();
    btn_prev_state = HIGH;
  
    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef UI_HAS_JOYSTICK
      pinMode(PIN_BACK_BTN, INPUT_PULLUP);
      pinMode(JOYSTICK_LEFT, INPUT_PULLUP);
      pinMode(JOYSTICK_RIGHT, INPUT_PULLUP);
      pinMode(PIN_USER_BTN, INPUT_PULLUP);
      joystick_left.begin();
      joystick_right.begin();
      user_btn.begin();
      back_btn.begin();
    #else if defined (BUTTON_PIN)
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
      Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
    #endif
    
    Wire.begin();

    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // give sx1262 some time to power up

    #ifdef STATUS_LED_LORA_ACTIVITY
      pinMode(PIN_LED, OUTPUT);
      setLed(false);
    #endif
}

#ifdef STATUS_LED_LORA_ACTIVITY

void PromicroBoard::setLed(bool on) {
    _led_on = on;
    digitalWrite(PIN_LED, on == (LED_STATE_ON != 0) ? HIGH : LOW);
}

void PromicroBoard::onBeforeTransmit() {
    _txing = true;
    setLed(true);
}

void PromicroBoard::onAfterTransmit() {
    _txing = false;
    // a short packet would otherwise flash too briefly to notice
    _led_off_at = millis() + STATUS_LED_TX_MILLIS;
}

void PromicroBoard::onLoRaPacketReceived() {
    if (_txing) return;

    uint32_t until = millis() + STATUS_LED_RX_MILLIS;
    if (_led_on && (int32_t)(until - _led_off_at) <= 0) return;  // don't shorten a longer pulse

    _led_off_at = until;
    setLed(true);
}

void PromicroBoard::updateStatusLed() {
    if (!_led_on || _txing) return;
    if ((int32_t)(millis() - _led_off_at) < 0) return;

    setLed(false);
}

#endif