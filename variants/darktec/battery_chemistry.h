#pragma once

/*
 * Химия батареи Darktec → диапазон % на UI и пороги power-management nRF52.
 *
 * Аппаратный лимит: напряжение пакета ≤ 5 В.
 *   Li-ion / LiFePO4 : только 1S
 *   LTO              : 1S или 2S (2S full = 5000 мВ)
 *
 * Выбор в platformio.ini, например:
 *   -D BATTERY_CHEMISTRY=BATTERY_CHEM_LIFEPO4
 *   -D BATTERY_CELLS=1
 *
 * Значения LPCOMP REFSEL для не-Li-ion — ближайшие оценки по эмпирической
 * калибровке класса ProMicro (11≈3.0 В, 12≈3.9 В, 4≈4.15 В);
 * их нужно проверить на железе (делитель Darktec: 100к/100к).
 */

#define BATTERY_CHEM_LIION    1
#define BATTERY_CHEM_LIFEPO4  2
#define BATTERY_CHEM_LTO      3

#ifndef BATTERY_CHEMISTRY
#define BATTERY_CHEMISTRY  BATTERY_CHEM_LIION
#endif

#ifndef BATTERY_CELLS
#define BATTERY_CELLS  1
#endif

#if BATTERY_CHEMISTRY != BATTERY_CHEM_LIION && \
    BATTERY_CHEMISTRY != BATTERY_CHEM_LIFEPO4 && \
    BATTERY_CHEMISTRY != BATTERY_CHEM_LTO
#error "BATTERY_CHEMISTRY должен быть BATTERY_CHEM_LIION, BATTERY_CHEM_LIFEPO4 или BATTERY_CHEM_LTO"
#endif

#if BATTERY_CELLS < 1
#error "BATTERY_CELLS должен быть >= 1"
#endif

#if BATTERY_CHEMISTRY != BATTERY_CHEM_LTO && BATTERY_CELLS > 1
#error "BATTERY_CELLS>1 недопустим для Li-ion/LiFePO4 на Darktec: вход батареи ограничен 5 В, поддерживается только 1S. Укажите BATTERY_CELLS=1 либо BATTERY_CHEM_LTO с BATTERY_CELLS=1 или 2."
#endif

#if BATTERY_CHEMISTRY == BATTERY_CHEM_LTO && BATTERY_CELLS > 2
#error "BATTERY_CELLS>2 недопустим для LTO на Darktec: вход батареи ограничен 5 В (LTO 2S full = 5000 мВ). Укажите BATTERY_CELLS=1 или BATTERY_CELLS=2."
#endif

#if BATTERY_CHEMISTRY == BATTERY_CHEM_LIION
#define BATT_CELL_EMPTY_MV     3000
#define BATT_CELL_FULL_MV      4200
#define BATT_CELL_BOOTLOCK_MV  3350
#define BATT_CELL_CRITICAL_MV  3000
#define BATT_CELL_WAKE_MV      3900
#define BATT_LPCOMP_LOW_REFSEL   11
#define BATT_LPCOMP_WAKE_REFSEL  12

#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LIFEPO4
#define BATT_CELL_EMPTY_MV     2700
#define BATT_CELL_FULL_MV      3650
#define BATT_CELL_BOOTLOCK_MV  3000
#define BATT_CELL_CRITICAL_MV  2700
#define BATT_CELL_WAKE_MV      3400
/* Нет эмпирической точки между ~3.0 В и ~3.9 В; wake=11 (пробуждение по USB всё равно работает). */
#define BATT_LPCOMP_LOW_REFSEL   11
#define BATT_LPCOMP_WAKE_REFSEL  11

#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LTO
#define BATT_CELL_EMPTY_MV     1900
#define BATT_CELL_FULL_MV      2500
#define BATT_CELL_BOOTLOCK_MV  2100
#define BATT_CELL_CRITICAL_MV  1900
#define BATT_CELL_WAKE_MV      2300
#if BATTERY_CELLS == 1
/* Оценка для диапазона ~2.0 В — проверить на железе. */
#define BATT_LPCOMP_LOW_REFSEL   10
#define BATT_LPCOMP_WAKE_REFSEL  10
#else
/* 2S: critical≈3800 → REFSEL 12; wake≈4600 → REFSEL 13 (оценка — проверить на железе). */
#define BATT_LPCOMP_LOW_REFSEL   12
#define BATT_LPCOMP_WAKE_REFSEL  13
#endif
#endif

#define BATT_PACK_EMPTY_MV     (BATT_CELL_EMPTY_MV    * BATTERY_CELLS)
#define BATT_PACK_FULL_MV      (BATT_CELL_FULL_MV     * BATTERY_CELLS)
#define BATT_PACK_BOOTLOCK_MV  (BATT_CELL_BOOTLOCK_MV * BATTERY_CELLS)
#define BATT_PACK_CRITICAL_MV  (BATT_CELL_CRITICAL_MV * BATTERY_CELLS)
#define BATT_PACK_WAKE_MV      (BATT_CELL_WAKE_MV     * BATTERY_CELLS)

#if BATT_PACK_FULL_MV > 5000
#error "Полное напряжение пакета превышает лимит входа батареи Darktec 5 В"
#endif

#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS  BATT_PACK_EMPTY_MV
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS  BATT_PACK_FULL_MV
#endif

#ifndef PWRMGT_VOLTAGE_BOOTLOCK
#define PWRMGT_VOLTAGE_BOOTLOCK  BATT_PACK_BOOTLOCK_MV
#endif
#ifndef PWRMGT_VOLTAGE_CRITICAL
#define PWRMGT_VOLTAGE_CRITICAL  BATT_PACK_CRITICAL_MV
#endif
#ifndef AUTO_SHUTDOWN_MILLIVOLTS
#define AUTO_SHUTDOWN_MILLIVOLTS  BATT_PACK_CRITICAL_MV
#endif

#ifndef PWRMGT_LPCOMP_LOW_REFSEL
#define PWRMGT_LPCOMP_LOW_REFSEL  BATT_LPCOMP_LOW_REFSEL
#endif
#ifndef PWRMGT_LPCOMP_REFSEL
#define PWRMGT_LPCOMP_REFSEL  BATT_LPCOMP_WAKE_REFSEL
#endif
