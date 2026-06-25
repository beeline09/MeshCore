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
#include <T114Board.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include <helpers/sensors/LocationProvider.h>

#ifdef DISPLAY_CLASS
#include <helpers/ui/MomentaryButton.h>
#ifdef HELTEC_T114_WITH_DISPLAY
#include <helpers/ui/ST7789Display.h>
#else
#include "helpers/ui/NullDisplayDriver.h"
#endif
#endif

extern T114Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
extern DISPLAY_CLASS display;
extern MomentaryButton user_btn;
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
