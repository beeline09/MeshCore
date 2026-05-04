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
#include "T1000eBoard.h"
#include <helpers/radiolib/CustomLR1110Wrapper.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#ifdef DISPLAY_CLASS
  #include "NullDisplayDriver.h"
#endif

class T1000SensorManager: public SensorManager {
  bool gps_active = false;
  LocationProvider * _nmea;

  void start_gps();
  void sleep_gps();
  void stop_gps();
public:
  T1000SensorManager(LocationProvider &nmea): _nmea(&nmea) { }
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void loop() override;
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
  LocationProvider* getLocationProvider() { return _nmea; }
};

#ifdef DISPLAY_CLASS
  extern NullDisplayDriver display;
#endif

extern T1000eBoard board;
extern WRAPPER_CLASS radio_driver;
extern VolatileRTCClock rtc_clock;
extern T1000SensorManager sensors;

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();
