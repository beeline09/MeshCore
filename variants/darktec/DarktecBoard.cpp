#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "DarktecBoard.h"
#include "DarktecPrefs.h"
#include "DarktecPowerState.h"
#include "DarktecSocModel.h"

void DarktecBoard::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    // Darktec settings: lazy FS mount + read /darktec.prefs (spec 0001
    // FR-14/D-7). Idempotent; the core mounts the same FS later on its own.
    DarktecPrefs::begin();

#if defined(DARKTEC_RADIO_EN_PIN)
    // E22P: EN (global RF-path switch) is held high from here on and never
    // touched again — DIO2 sets the direction (the stock RadioLib RXEN
    // driving dropped it on TX and killed the path).
    pinMode(DARKTEC_RADIO_EN_PIN, OUTPUT);
    digitalWrite(DARKTEC_RADIO_EN_PIN, HIGH);
#endif

    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
      Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
    #endif

    Wire.begin();

    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);

    _ina_ok = _ina.begin(TELEM_INA3221_ADDRESS, &Wire);
    if (_ina_ok) {
      _ina.setShuntResistance(DARKTEC_INA_CHARGE_CH, TELEM_INA3221_SHUNT_VALUE);
    }
}

// ---- SOC model and charge automat (spec 0001, FR-21..27, FR-34) ----
static DarktecSocModel::State g_soc;
static DarktecSocModel::ChargeSm g_charge;
static DarktecSocModel::Config g_soc_cfg;
static bool g_soc_cfg_ready = false;

static void ensureSocCfg() {
  if (g_soc_cfg_ready) return;
  g_soc_cfg_ready = true;
  const auto& prefs = DarktecPrefs::get();
  const DarktecSoc::Chem chem =
      (prefs.chem == DarktecPrefsFormat::CHEM_LIFEPO4) ? DarktecSoc::Chem::LiFePO4
      : (prefs.chem == DarktecPrefsFormat::CHEM_LTO)   ? DarktecSoc::Chem::LTO
                                                       : DarktecSoc::Chem::LiIon;
  g_soc_cfg = DarktecSocModel::configFor(chem, prefs.cells, prefs.capacity_mah);
}

void DarktecBoard::pollChargeSense() {
  uint32_t now = millis();
  if (_ina_last_ms != 0 && (now - _ina_last_ms) < 250) {
    return;
  }
  _ina_last_ms = now;

  if (!_ina_ok) {
    _ina_ok = _ina.begin(TELEM_INA3221_ADDRESS, &Wire);
    if (!_ina_ok) {
      return;
    }
    _ina.setShuntResistance(DARKTEC_INA_CHARGE_CH, TELEM_INA3221_SHUNT_VALUE);
  }

  float amps = _ina.getCurrentAmps(DARKTEC_INA_CHARGE_CH);
  if (isnan(amps)) {
    return;
  }
  const int32_t ma = (int32_t)(amps * 1000.0f);

  // SOC model / charge automat tick (FR-21..27, FR-34). Positive current
  // means charging (INA3221 CH2 is the charge channel on this board).
  ensureSocCfg();
  const uint16_t mv = readBattMvRaw();
  if (!g_soc.started) {
    DarktecSocModel::reset(g_soc, g_soc_cfg, mv, now);
  } else {
    DarktecSocModel::update(g_soc, g_soc_cfg, DarktecSocModel::restPolicy(), mv, ma, now);
  }
  DarktecSocModel::updateCharge(g_charge, g_soc_cfg, DarktecSocModel::chargePolicy(),
                                (uint16_t)(mv / g_soc_cfg.cells), ma, now);
  _charging = (g_charge.state != DarktecSocModel::ChargeState::Idle);
}

bool DarktecBoard::isCharging() {
  pollChargeSense();
  return _charging;
}

bool DarktecBoard::isChargeFull() {
  pollChargeSense();
  return g_charge.state == DarktecSocModel::ChargeState::Full;
}

bool DarktecBoard::isExternalPowered() {
  // Pack charging is INA3221 CH2. USB VBUS is not pack charge, but KEEP_DISPLAY
  // and the stock charging icon also treat a USB cable as external power.
  return isCharging() || NRF52Board::isExternalPowered();
}

// ---- DarktecPowerState (facade for the UI overlay, spec 0001 FR-28) ----

uint8_t DarktecPowerState::socPercent(uint16_t fallback_pack_mv) {
  ensureSocCfg();
  if (g_soc.started) {
    return DarktecSocModel::percent(g_soc);
  }
  return DarktecSoc::socFromCellMv(g_soc_cfg.chem,
                                   (uint16_t)(fallback_pack_mv / g_soc_cfg.cells));
}

bool DarktecPowerState::chargeActive() {
  ensureSocCfg();
  return g_charge.state != DarktecSocModel::ChargeState::Idle;
}
