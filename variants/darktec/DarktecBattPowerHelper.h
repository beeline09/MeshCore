#pragma once

/*
 * Локальный хелпер Darktec: согласовать порог critical химии с базовым
 * NRF52Board::lpcompDownHandler(), где зашито сравнение mv < 3000.
 *
 * src/helpers/NRF52Board.cpp менять нельзя. Поэтому, когда обработчик
 * вызывает getBattMilliVolts() из контекста IRQ, подменяем значение так,
 * чтобы сохранилась семантика PWRMGT_VOLTAGE_CRITICAL, не портя телеметрию.
 *
 * В IRQ (LPCOMP DOWN):
 *   mv >= critical && mv < 3000  → вернуть 3000 (не глушить ложно при USB)
 *   mv <  critical               → вернуть mv   (разрешить shutdown через < 3000)
 * Вне IRQ: всегда реальные милливольты с АЦП.
 */

#include <Arduino.h>

#ifndef PWRMGT_VOLTAGE_CRITICAL
#define PWRMGT_VOLTAGE_CRITICAL 3000
#endif

namespace darktec {

inline bool inInterruptContext() {
  return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

inline uint16_t adjustBattMilliVoltsForPowerMgr(uint16_t mv) {
  if (!inInterruptContext()) {
    return mv;
  }
  if (mv >= (uint16_t)PWRMGT_VOLTAGE_CRITICAL && mv < 3000) {
    return 3000;
  }
  return mv;
}

}  // namespace darktec
