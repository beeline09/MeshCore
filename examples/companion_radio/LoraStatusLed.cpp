#include "LoraStatusLed.h"

#if defined(STATUS_LED_LORA_ACTIVITY) && defined(PIN_STATUS_LED)

LoraStatusLed lora_status_led;

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

void LoraStatusLed::begin() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LED_STATE_ON ? LOW : HIGH);
  _on = false;
  _off_at = 0;
}

void LoraStatusLed::pulse(unsigned long millis_on) {
  unsigned long until = millis() + millis_on;

  // a longer pulse already in flight (TX) must not be cut short by a shorter one (RX)
  if (_on && (long)(until - _off_at) <= 0) return;

  _off_at = until;
  if (!_on) {
    _on = true;
    digitalWrite(PIN_STATUS_LED, LED_STATE_ON ? HIGH : LOW);
  }
}

void LoraStatusLed::loop() {
  if (!_on) return;
  if ((long)(millis() - _off_at) < 0) return;

  _on = false;
  digitalWrite(PIN_STATUS_LED, LED_STATE_ON ? LOW : HIGH);
}

#endif
