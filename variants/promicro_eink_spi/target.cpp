#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

PromicroEinkBoard board;

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);

WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  #if UI_HAS_JOYSTICK
    // SELECT (D6): long press = back/cancel. No multi-click wait for faster response.
    MomentaryButton user_btn(KEY_SELECT, 1000, true, true, false);
    // Direction buttons: no long press, no multi-click → near-instant CLICK events.
    MomentaryButton joystick_left(KEY_LEFT, 0, true, true, false);
    MomentaryButton joystick_right(KEY_RIGHT, 0, true, true, false);
    MomentaryButton back_btn(KEY_UP, 0, true, true, false);
    // KEY_DOWN = D17 = P0.31, shared with battery ADC. Digital reads work fine
    // alongside periodic analogRead(); the pin state is independent of SAADC.
    MomentaryButton joystick_down(KEY_DOWN, 0, 250, false);
    //MomentaryButton joystick_down(KEY_DOWN, 0, true, true, false);
  #else
    MomentaryButton user_btn(KEY_SELECT, 1000, true, true);
  #endif
#endif

bool radio_init() {
  bool ok = radio.std_init(&SPI);
#ifdef DISPLAY_CLASS
  if (!ok) {
    display.startFrame();
    display.setCursor(0, 28);
    display.print("Radio FAILED");
    display.setCursor(0, 48);
    display.print("see serial");
    display.endFrame();
  }
#endif
  return ok;
}

uint32_t radio_get_rng_seed() {
  return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio.setFrequency(freq);
  radio.setSpreadingFactor(sf);
  radio.setBandwidth(bw);
  radio.setCodingRate(cr);
}

void radio_set_tx_power(int8_t dbm) {
  radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
