#pragma once

/*
 * Darktec radio defaults (new prefs / erase FS).
 * Overrides LORA_* from [arduino_base]: 869.075 MHz, BW 62.5, SF8, CR8, 22 dBm.
 *
 * On-demand / lab: DARKTEC_RADIO_CUSTOM + LORA_* come from a generated
 * -include header (not -U/-D in PLATFORMIO_BUILD_FLAGS — SCons moves -U
 * to the end of argv).
 */

#ifdef DARKTEC_RADIO_CUSTOM

#ifndef LORA_FREQ
#define LORA_FREQ  869.075
#endif
#ifndef LORA_BW
#define LORA_BW    62.5
#endif
#ifndef LORA_SF
#define LORA_SF    8
#endif
#ifndef LORA_CR
#define LORA_CR    8
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER  22
#endif

#else /* !DARKTEC_RADIO_CUSTOM */

#ifdef LORA_FREQ
#undef LORA_FREQ
#endif
#define LORA_FREQ  869.075

#ifdef LORA_BW
#undef LORA_BW
#endif
#define LORA_BW    62.5

#ifdef LORA_SF
#undef LORA_SF
#endif
#define LORA_SF    8

#ifdef LORA_CR
#undef LORA_CR
#endif
#define LORA_CR    8

#ifdef LORA_TX_POWER
#undef LORA_TX_POWER
#endif
#define LORA_TX_POWER  22

#endif /* DARKTEC_RADIO_CUSTOM */
