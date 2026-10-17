#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <nrf_gpio.h>

#include "DarktecBoard.h"
#include "DarktecPrefs.h"
#include "DarktecPowerState.h"
#include "DarktecSocModel.h"

#ifdef NRF52_POWER_MANAGEMENT
const PowerMgtConfig power_config = {
  .lpcomp_ain_channel = PWRMGT_LPCOMP_AIN,
  .lpcomp_refsel      = PWRMGT_LPCOMP_REFSEL,
  .voltage_bootlock   = PWRMGT_VOLTAGE_BOOTLOCK,
  .lpcomp_low_refsel  = PWRMGT_LPCOMP_LOW_REFSEL,
};

void DarktecBoard::initiateShutdown(uint8_t reason) {
#if DARKTEC_BATT_PROTECT == DARKTEC_BATT_PROTECT_ADC
  if (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
      reason == SHUTDOWN_REASON_BOOT_PROTECT) {
    darktec::waitForBatteryRecovery(*this, SX126X_POWER_EN);
    return;  // недостижимо
  }
#endif

  digitalWrite(SX126X_POWER_EN, LOW);
  enterSystemOff(reason);
}
#endif

void DarktecBoard::powerOff() {
  // Hibernate / CLI — SYSTEMOFF, как у Rogovogor.
  // ADC-wait только для защиты батареи (initiateShutdown LOW_VOLTAGE/BOOT_PROTECT).
#ifdef PIN_USER_BTN
  while (digitalRead(PIN_USER_BTN) == LOW) {
    delay(10);
  }
#endif

  shutdownPeripherals();
  digitalWrite(SX126X_POWER_EN, LOW);

#ifdef PIN_USER_BTN
  nrf_gpio_cfg_sense_input(g_ADigitalPinMap[PIN_USER_BTN], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
#endif

#ifdef NRF52_POWER_MANAGEMENT
  enterSystemOff(SHUTDOWN_REASON_USER);
#else
  NRF52Board::powerOff();
#endif
}

void DarktecBoard::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    // Настройки Darktec: ленивое монтирование ФС + чтение /darktec.prefs
    // (spec 0001 FR-14/D-7). Идемпотентно; ядро смонтирует ФС позже ещё раз.
    DarktecPrefs::begin();

#if defined(DARKTEC_RADIO_EN_PIN)
    // E22P: EN (общий включатель RF-тракта) держим высоким до radio_init()
    // и больше не трогаем — направление задаёт DIO2 (замеры MacMillan25:
    // приём/передача/покой — везде 3.3 В, усилитель ожил).
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
#ifdef NRF52_POWER_MANAGEMENT
#if DARKTEC_BATT_PROTECT == DARKTEC_BATT_PROTECT_ADC
    // Boot-lock по АЦП пакета (химия): ждём подъёма VBAT на делителе.
    if (darktec::batteryBelowThreshold(*this, PWRMGT_VOLTAGE_BOOTLOCK)) {
      darktec::waitForBatteryRecovery(*this, SX126X_POWER_EN);
    }
#endif
    // LPCOMP runtime alert не включаем (lpcomp_low_refsel=0).
    (void)power_config;
#endif
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // дать sx1262 время на включение

    _ina_ok = _ina.begin(TELEM_INA3221_ADDRESS, &Wire);
    if (_ina_ok) {
      _ina.setShuntResistance(DARKTEC_INA_CHARGE_CH, TELEM_INA3221_SHUNT_VALUE);
    }
}

// ---- SOC-модель и автомат заряда (spec 0001, FR-21..27, FR-34) ----
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

  // Тик SOC-модели и автомата заряда (FR-21..27, FR-34). Знак «+» = заряд
  // (канал CH2 — зарядный, по схеме платы).
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
  // Зарядка пакета — INA3221 CH2. USB VBUS MCU к зарядке не относится,
  // но для KEEP_DISPLAY / CLI «external» учитываем и его (DFU, кабель).
  return isCharging() || NRF52Board::isExternalPowered();
}

// ---- DarktecPowerState (фасад для оверлея BatteryPack) ----

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
