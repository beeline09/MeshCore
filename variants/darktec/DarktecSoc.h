#pragma once
// Darktec SOC: чистая функция «напряжение покоя ячейки → SOC %» (spec 0001).
// Только таблица и интерполяция — без железа; кулонометр/R-компенсация/автомат
// заряда — надстройки (FR-20..27). Таблицы генерируются
// tools/gen_darktec_soc_tables.py (DarktecSocTables.h, руками не править).

#include <stdint.h>

#include "DarktecSocTables.h"

namespace DarktecSoc {

// Химии, поддерживаемые вариантом Darktec (spec 0001 §8).
enum class Chem : uint8_t { LiIon = 0, LiFePO4 = 1, LTO = 2 };

// SOC (0..100 %) по напряжению покоя одной ЯЧЕЙКИ (мВ), линейная интерполяция.
// Вне диапазона таблицы — клампится в 0/100. На плоских сегментах (LiFePO4
// плато) значение по напряжению неточно по физике — точность обеспечивает
// кулонометр (FR-22). Для пакета из N ячеек caller делит напряжение пакета
// на N (FR-26).
inline uint8_t socFromCellMv(Chem chem, uint16_t cell_mv) {
  const uint16_t* table = (chem == Chem::LiIon)   ? DarktecSocTables::kLiion
                          : (chem == Chem::LiFePO4) ? DarktecSocTables::kLifepo4
                                                    : DarktecSocTables::kLto;
  if (cell_mv <= table[0]) return 0;
  if (cell_mv >= table[DARKTEC_SOC_POINTS - 1]) return 100;
  // Таблица неубывает по индексу; ищем первый сегмент, накрывающий cell_mv.
  for (int i = 1; i < DARKTEC_SOC_POINTS; i++) {
    if (cell_mv < table[i]) {
      const uint16_t v0 = table[i - 1];
      const uint16_t v1 = table[i];
      if (v1 <= v0) {
        // Плоский сегмент не может накрывать строго меньшее значение —
        // защита от кривой таблицы: левая граница сегмента.
        return (uint8_t)((i - 1) * DARKTEC_SOC_STEP_PCT);
      }
      const uint32_t soc = (uint32_t)(i - 1) * DARKTEC_SOC_STEP_PCT +
                           ((uint32_t)(cell_mv - v0) * DARKTEC_SOC_STEP_PCT) /
                               (v1 - v0);
      return (uint8_t)soc;
    }
  }
  return 100;
}

}  // namespace DarktecSoc
