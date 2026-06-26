#include "BuildLabScoutBoard.h"

void BuildLabScoutBoard::begin() {
  ESP32Board::begin();

#ifdef PIN_GPS_EN
  // MeshCore's generic UART-GPS init (initBasicGPS) never drives PIN_GPS_EN,
  // so the module would stay off. Drive it active-high (matches Meshtastic GPS_EN_ACTIVE 1).
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, HIGH);
#endif
}

uint16_t BuildLabScoutBoard::getBattMilliVolts() {
#ifdef PIN_VBAT_READ
  analogReadResolution(12);

  uint32_t raw = 0;
  for (int i = 0; i < 8; i++) {
    raw += analogReadMilliVolts(PIN_VBAT_READ);
  }
  raw = raw / 8;

  return (uint16_t)(raw * BATTERY_ADC_MULTIPLIER);
#else
  return 0;  // not supported
#endif
}
