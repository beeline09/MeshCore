#pragma once
// SOC-модель Darktec: кулонометр + дрейф-коррекция по таблице в покое
// (spec 0001, FR-21..24, D-1..D-4) и автомат заряда (FR-34).
// Чистая логика без железа — часы подаются параметром (native-тесты, NFR-4).
//
// Единицы: напряжение пакета — мВ; ток — мА, знак «+» = заряд (в батарею).
// SOC хранится с точностью 0.01 % (soc_x100: 0..10000).

#include <stdint.h>

#include "DarktecSoc.h"

namespace DarktecSocModel {

struct Config {
  DarktecSoc::Chem chem;
  uint8_t cells;         // 1..2 (LTO 2S)
  uint16_t capacity_mah; // ёмкость ЯЧЕЙКИ (FR-1)
  uint16_t r_cell_mohm;  // внутреннее сопротивление ячейки (D-1: 60/40/30)
};

inline Config configFor(DarktecSoc::Chem chem, uint8_t cells,
                        uint16_t capacity_mah) {
  Config c{};
  c.chem = chem;
  c.cells = cells < 1 ? 1 : cells;
  c.capacity_mah = capacity_mah;
  // D-1: средние типовые из открытых источников; уточнение замером sag.
  switch (chem) {
    case DarktecSoc::Chem::LiFePO4: c.r_cell_mohm = 40; break;
    case DarktecSoc::Chem::LTO:     c.r_cell_mohm = 30; break;
    default:                        c.r_cell_mohm = 60; break;
  }
  return c;
}

// Пороги «покоя» (D-2): |I| < rest_ma непрерывно rest_minutes → таблица
// корректирует кулонометр; скорость коррекции ≤ drift_rate (x100 SOC/час).
struct RestPolicy {
  int16_t rest_ma;
  uint16_t rest_minutes;
  uint16_t drift_rate_x100_per_hour;
};
inline RestPolicy restPolicy() {
  return RestPolicy{/*rest_ma*/ 10, /*rest_minutes*/ 5, /*rate*/ 500};
}

struct State {
  int32_t soc_x100;         // 0..10000
  int32_t coulomb_rem;      // остаток числителя кулонометра (точность на тике)
  uint32_t last_ms;
  int32_t last_ma;          // последний ток (мА, «+» заряд)
  uint32_t rest_since_ms;   // с какого момента в покое; 0 = не в покое
  int16_t temp_deci_c;      // сглаженная температура кристалла ×10 (FR-24)
  bool temp_valid;
  bool started;             // false до первого тика
  int64_t drift_accum;      // накопитель бюджета дрейфа (x100·мс)
};

inline void reset(State& st, const Config& cfg, uint16_t pack_mv,
                  uint32_t now_ms) {
  st = State{};
  st.soc_x100 = (int32_t)DarktecSoc::socFromCellMv(
                    cfg.chem, (uint16_t)(pack_mv / cfg.cells)) * 100;
  st.last_ms = now_ms;
  st.started = true;
}

// Один тик модели: пакет мВ + ток мА («+» заряд) + время.
inline void update(State& st, const Config& cfg, const RestPolicy& rest,
                   uint16_t pack_mv, int32_t current_ma, uint32_t now_ms) {
  const bool quiet = (current_ma > -rest.rest_ma) && (current_ma < rest.rest_ma);

  if (!st.started) {  // первый тик — только якорим время и покой
    st.started = true;
    st.last_ms = now_ms;
    st.last_ma = current_ma;
    st.rest_since_ms = quiet ? now_ms : 0;
    return;
  }
  uint32_t dt = now_ms - st.last_ms;
  st.last_ms = now_ms;
  st.last_ma = current_ma;
  if (dt == 0) return;
  if (dt > 3600000u) dt = 3600000u;  // защита от скачков часов

  // 1) Кулонометр: ΔSOC_x100 = I[mA]·dt[мс] / (360·capacity[mAh]) — с
  // остаточным аккумулятором, чтобы не терять дольки на коротких тиках
  // (при тике 250 мс и 200 мА шаг ≈ 0.03 x100 — целочисленное деление
  // без остатка обнулило бы счёт).
  {
    const int64_t num = (int64_t)current_ma * (int64_t)dt;
    const int64_t den = 360LL * cfg.capacity_mah;
    st.soc_x100 += (int32_t)(num / den);
    st.coulomb_rem += (int32_t)(num % den);
    const int64_t den32 = den;
    while (st.coulomb_rem >= den32) {
      st.soc_x100 += 1;
      st.coulomb_rem -= (int32_t)den32;
    }
    while (st.coulomb_rem <= -den32) {
      st.soc_x100 -= 1;
      st.coulomb_rem += (int32_t)den32;
    }
  }

  // 2) Дрейф-коррекция по напряжению — только в покое (D-2).
  if (quiet && !st.rest_since_ms) st.rest_since_ms = now_ms;
  if (!quiet) {
    st.rest_since_ms = 0;
    st.drift_accum = 0;
  }

  if (quiet && st.rest_since_ms &&
      (now_ms - st.rest_since_ms) >= (uint32_t)rest.rest_minutes * 60000u) {
    const uint16_t cell_mv = (uint16_t)(pack_mv / cfg.cells);
    const int32_t v_soc = (int32_t)DarktecSoc::socFromCellMv(cfg.chem, cell_mv) * 100;
    const int32_t diff = v_soc - st.soc_x100;
    if (diff != 0) {
      // Бюджет дрейфа копится отдельно: за один короткий тик разрешённый
      // шаг (rate·dt/час) почти всегда < 1 x100 — без накопителя дрейф бы
      // не двигался вообще.
      st.drift_accum += (int64_t)rest.drift_rate_x100_per_hour * (int64_t)dt;
      int64_t step = st.drift_accum / 3600000LL;
      if (step > 0) {
        const int64_t applied =
            (diff > 0) ? (diff < step ? diff : step) : (-diff < step ? -diff : step);
        st.soc_x100 += (diff > 0) ? (int32_t)applied : -(int32_t)applied;
        st.drift_accum -= applied * 3600000LL;
      }
    }
  }

  if (st.soc_x100 < 0) st.soc_x100 = 0;
  if (st.soc_x100 > 10000) st.soc_x100 = 10000;
}

inline uint8_t percent(const State& st) { return (uint8_t)(st.soc_x100 / 100); }

// ---- Автомат заряда (FR-34) ----

enum class ChargeState : uint8_t { Idle = 0, Charging = 1, Full = 2 };

struct ChargePolicy {
  int16_t enter_ma;      // порог входа в charging
  int16_t exit_ma;       // порог выхода (гистерезис)
  uint16_t term_div;     // полный: I < capacity/term_div (C/30)
  uint16_t full_eps_mv;  // V ячейки в пределах V_full − eps
  uint16_t dwell_min;    // сколько минут держится условие full
};
inline ChargePolicy chargePolicy() {
  return ChargePolicy{/*enter*/ 30, /*exit*/ 15, /*div*/ 30, /*eps*/ 15,
                      /*dwell*/ 3};
}

struct ChargeSm {
  ChargeState state = ChargeState::Idle;
  uint32_t full_since_ms = 0;  // с какого момента выполняется условие full
};

// Таблица задаёт V_full ячейки (верхняя точка) — из DarktecSocTables.
inline uint16_t fullCellMv(DarktecSoc::Chem chem) {
  const uint16_t* table = (chem == DarktecSoc::Chem::LiIon)  ? DarktecSocTables::kLiion
                          : (chem == DarktecSoc::Chem::LiFePO4) ? DarktecSocTables::kLifepo4
                                                                : DarktecSocTables::kLto;
  return table[DARKTEC_SOC_POINTS - 1];
}

// Тик автомата: v_cell — напряжение ячейки, current_ma — «+» заряд.
inline void updateCharge(ChargeSm& sm, const Config& cfg,
                         const ChargePolicy& pol, uint16_t v_cell,
                         int32_t current_ma, uint32_t now_ms) {
  const uint16_t v_full = fullCellMv(cfg.chem);
  const int32_t term_ma = (int32_t)cfg.capacity_mah / (int32_t)pol.term_div;

  const bool near_full_v = (v_cell + pol.full_eps_mv >= v_full);
  const bool below_term = (current_ma < term_ma);

  switch (sm.state) {
    case ChargeState::Idle:
      if (current_ma > pol.enter_ma) {
        sm.state = ChargeState::Charging;
        sm.full_since_ms = 0;
      }
      break;

    case ChargeState::Charging: {
      // Кандидат на full: V у CV-порога и ток ниже C/30 — держится dwell_min.
      if (near_full_v && below_term) {
        if (sm.full_since_ms == 0) sm.full_since_ms = now_ms;
        if (now_ms - sm.full_since_ms >= (uint32_t)pol.dwell_min * 60000u) {
          sm.state = ChargeState::Full;
        }
        break;
      }
      sm.full_since_ms = 0;
      // Выход из charging — только при спаде ниже exit (гистерезис) и не у
      // CV-порога (там ток закономерно мал — это ещё зарядка, FR-34).
      if (current_ma < pol.exit_ma && !near_full_v) {
        sm.state = ChargeState::Idle;
      }
      break;
    }

    case ChargeState::Full:
      // Выход из full: началась новая CC-фаза (ток выше C/30 — после
      // «полного» подпитка ниже) либо напряжение ощутимо просело (сняли
      // зарядку, ячейка отдаёт). CV-подпитка между exit и C/30 — остаёмся
      // в full, иначе осцилляция full↔charging (FR-34).
      if (current_ma > term_ma) {
        sm.state = ChargeState::Charging;
        sm.full_since_ms = 0;
      } else if (v_cell + 2 * pol.full_eps_mv < v_full) {
        sm.state = ChargeState::Idle;
        sm.full_since_ms = 0;
      }
      break;
  }
}

}  // namespace DarktecSocModel
