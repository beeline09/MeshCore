#pragma once

/*
 * Battery chemistry for Darktec UI percentage (BATT_MIN/MAX_MILLIVOLTS).
 * Pack voltage must stay ≤ 5 V: Li-ion / LiFePO4 = 1S only; LTO = 1S or 2S.
 *
 *   -D BATTERY_CHEMISTRY=BATTERY_CHEM_LIION | BATTERY_CHEM_LIFEPO4 | BATTERY_CHEM_LTO
 *   -D BATTERY_CELLS=1
 *
 * On-demand / lab: override via -include (not -U/-D in PLATFORMIO_BUILD_FLAGS).
 * Battery hard-cutoff / ADC sleep is intentionally not in this official port.
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
#error "BATTERY_CHEMISTRY must be BATTERY_CHEM_LIION, BATTERY_CHEM_LIFEPO4 or BATTERY_CHEM_LTO"
#endif

#if BATTERY_CELLS < 1
#error "BATTERY_CELLS must be >= 1"
#endif

#if BATTERY_CHEMISTRY != BATTERY_CHEM_LTO && BATTERY_CELLS > 1
#error "BATTERY_CELLS>1 is not allowed for Li-ion/LiFePO4 on Darktec (pack input ≤ 5 V, 1S only)"
#endif

#if BATTERY_CHEMISTRY == BATTERY_CHEM_LTO && BATTERY_CELLS > 2
#error "BATTERY_CELLS>2 is not allowed for LTO on Darktec (2S full = 5000 mV)"
#endif

#if BATTERY_CHEMISTRY == BATTERY_CHEM_LIION
#define BATT_CELL_EMPTY_MV  3000
#define BATT_CELL_FULL_MV   4200
#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LIFEPO4
#define BATT_CELL_EMPTY_MV  2500
#define BATT_CELL_FULL_MV   3650
#elif BATTERY_CHEMISTRY == BATTERY_CHEM_LTO
#define BATT_CELL_EMPTY_MV  1800
#define BATT_CELL_FULL_MV   2500
#endif

#define BATT_PACK_EMPTY_MV  (BATT_CELL_EMPTY_MV * BATTERY_CELLS)
#define BATT_PACK_FULL_MV   (BATT_CELL_FULL_MV  * BATTERY_CELLS)

#if BATT_PACK_FULL_MV > 5000
#error "Full pack voltage exceeds Darktec 5 V battery input limit"
#endif

#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS  BATT_PACK_EMPTY_MV
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS  BATT_PACK_FULL_MV
#endif
