#pragma once

#include <Arduino.h>

/**
 * Drives the single status LED from LoRa traffic instead of the UI heartbeat:
 * dark while idle, short flash on every received packet, longer flash on transmit.
 *
 * Enable with -D STATUS_LED_LORA_ACTIVITY. Without it this file compiles to nothing
 * and the stock heartbeat in UITask stays in charge.
 */
#if defined(STATUS_LED_LORA_ACTIVITY) && defined(PIN_STATUS_LED)

#ifndef STATUS_LED_RX_MILLIS
  #define STATUS_LED_RX_MILLIS 60
#endif

#ifndef STATUS_LED_TX_MILLIS
  #define STATUS_LED_TX_MILLIS 250
#endif

class LoraStatusLed {
public:
  void begin();

  void noteRx() { pulse(STATUS_LED_RX_MILLIS); }
  void noteTx() { pulse(STATUS_LED_TX_MILLIS); }

  void loop();

private:
  void pulse(unsigned long millis_on);

  bool          _on = false;
  unsigned long _off_at = 0;
};

extern LoraStatusLed lora_status_led;

#endif
