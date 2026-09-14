#pragma once

/*
 * Included via PLATFORMIO_BUILD_FLAGS -include for release / on-demand builds.
 * Battery, ADC_MULTIPLIER and radio overrides belong in a generated
 * -include header (see radio_defaults.h / variant.h). This file only
 * silences debug flags from platformio.ini.
 */

#ifdef MESH_DEBUG
#undef MESH_DEBUG
#endif

#ifdef BLE_DEBUG_LOGGING
#undef BLE_DEBUG_LOGGING
#endif
