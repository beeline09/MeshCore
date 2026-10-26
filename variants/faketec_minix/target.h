#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <PromicroBoard.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>

// Подменяем обёртку на ту, что сообщает о приёме светодиоду платы. Делается здесь, а не
// через build_flags: PlatformIO выводит -U после всех -D, поэтому переопределить макрос
// из platformio.ini невозможно.
#ifdef STATUS_LED_LORA_ACTIVITY
  #include <FaketecSX1262Wrapper.h>
  #undef WRAPPER_CLASS
  #define WRAPPER_CLASS FaketecSX1262Wrapper
#endif
#ifdef DISPLAY_CLASS
  #include <helpers/ui/SSD1306Display.h>
  #include <helpers/ui/MomentaryButton.h>
#endif

#include <helpers/sensors/EnvironmentSensorManager.h>

extern PromicroBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
    #ifdef UI_HAS_JOYSTICK
    extern MomentaryButton user_btn;
    extern MomentaryButton joystick_left;
    extern MomentaryButton joystick_right;
    extern MomentaryButton joystick_down;
    extern MomentaryButton back_btn;
  #else
    extern MomentaryButton user_btn;
  #endif
#endif

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();

