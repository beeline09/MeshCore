#pragma once

/*
 * Химия батареи Darktec → диапазон % на UI (BATT_MIN/MAX).
 *
 * Аппаратный лимит: напряжение пакета ≤ 5 В.
 *   Li-ion / LiFePO4 : только 1S
 *   LTO              : 1S или 2S (2S full = 5000 мВ)
 *
 * Выбор в platformio.ini, например:
 *   -D BATTERY_CHEMISTRY=BATTERY_CHEM_LIFEPO4
 *   -D BATTERY_CELLS=1
 *
 * Важно (cutoff / recovery):
 * Жёсткий SYSTEMOFF через boot-lock и LPCOMP DOWN на этой плате с boost/DCDC
 * блокирует возврат к жизни при подъёме напряжения пакета — оживает только USB.
 * Поэтому boot-lock и runtime LPCOMP отключены (0). Пороги химии используются
 * для шкалы % на дисплее, без аппаратного «кирпича».
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

#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LIFEPO4
#define BATT_CELL_EMPTY_MV     2500
#define BATT_CELL_FULL_MV      3650

#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LTO
#define BATT_CELL_EMPTY_MV     1800
#define BATT_CELL_FULL_MV      2500
#endif

#define BATT_PACK_EMPTY_MV  (BATT_CELL_EMPTY_MV * BATTERY_CELLS)
#define BATT_PACK_FULL_MV   (BATT_CELL_FULL_MV  * BATTERY_CELLS)

#if BATT_PACK_FULL_MV > 5000
#error "Полное напряжение пакета превышает лимит входа батареи Darktec 5 В"
#endif

#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS  BATT_PACK_EMPTY_MV
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS  BATT_PACK_FULL_MV
#endif

/* 0 = boot-lock выключен (иначе SYSTEMOFF + LPCOMP мешают подъёму с boost). */
#ifndef PWRMGT_VOLTAGE_BOOTLOCK
#define PWRMGT_VOLTAGE_BOOTLOCK  0
#endif

/* Критический порог только для справочной семантики хелпера; hard cutoff выключен. */
#ifndef PWRMGT_VOLTAGE_CRITICAL
#define PWRMGT_VOLTAGE_CRITICAL  BATT_PACK_EMPTY_MV
#endif

/* 0 = runtime LPCOMP DOWN выключен. */
#ifndef PWRMGT_LPCOMP_LOW_REFSEL
#define PWRMGT_LPCOMP_LOW_REFSEL  0
#endif

/* Wake REFSEL не используется без low-alert; оставляем безопасный дефолт. */
#ifndef PWRMGT_LPCOMP_REFSEL
#define PWRMGT_LPCOMP_REFSEL  0
#endif
