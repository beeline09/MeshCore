#pragma once
// POSIX timezone string for the clock display page.
// Examples: "UTC0"           UTC
//           "MSK-3"          Moscow (UTC+3)
//           "EET-2"          Eastern Europe (UTC+2)
//           "CET-1CEST,M3.5.0,M10.5.0/3"  Central Europe with DST
// Override in platformio.ini: -DDISPLAY_TZ="MSK-3"
// See: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
#ifndef DISPLAY_TZ
#  define DISPLAY_TZ  "UTC0"
#endif


#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <PromicroEinkBoard.h>
#if defined(USE_LLCC68)
  #include <helpers/radiolib/CustomLLCC68Wrapper.h>
#else
  #include <helpers/radiolib/CustomSX1262Wrapper.h>
#endif
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

#ifdef DISPLAY_CLASS
  #include <helpers/ui/GxEPDDisplay.h>
  #include <helpers/ui/MomentaryButton.h>
#endif

extern PromicroEinkBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
  #if UI_HAS_JOYSTICK
    extern MomentaryButton joystick_left;
    extern MomentaryButton joystick_right;
    extern MomentaryButton back_btn;
    extern MomentaryButton joystick_down;
  #endif
#endif

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();
