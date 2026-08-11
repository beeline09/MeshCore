#pragma once

/*
 * Предустановки радио Darktec (новые prefs / erase FS).
 * Только variants/darktec — переопределяет LORA_* из [arduino_base].
 *
 *   freq 869.075 МГц, BW 62.5 кГц, SF 8, CR 8, TX 22 дБм
 *
 * path.hash.mode (2 байта / 32 хопа) здесь не задаётся: это поле prefs
 * в examples/, без правок ядра/приложений — только CLI:
 *   set path.hash.mode 1
 */

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
