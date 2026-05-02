#pragma once
#include <stdint.h>

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1
#define ADVERT_LOC_PREFS      2

#define LOOP_DETECT_OFF       0
#define LOOP_DETECT_MINIMAL   1
#define LOOP_DETECT_MODERATE  2
#define LOOP_DETECT_STRICT    3

#define TELEM_MODE_DENY         0
#define TELEM_MODE_ALLOW_FLAGS  1
#define TELEM_MODE_ALLOW_ALL    2

struct NodePrefs {
  // === Common fields ===
  float    airtime_factor;
  char     node_name[32];
  float    freq;
  float    bw;
  uint8_t  sf;
  uint8_t  cr;
  int8_t   tx_power_dbm;
  uint8_t  multi_acks;
  float    rx_delay_base;
  uint8_t  rx_boosted_gain;
  uint8_t  path_hash_mode;
  uint8_t  gps_enabled;
  uint32_t gps_interval;
  uint8_t  advert_loc_policy;

  // === Repeater / CommonCLI fields ===
  double   node_lat, node_lon;
  char     password[16];
  uint8_t  disable_fwd;
  uint8_t  advert_interval;       // minutes / 2
  uint8_t  flood_advert_interval; // hours
  float    tx_delay_factor;
  char     guest_password[16];
  float    direct_tx_delay_factor;
  uint32_t guard;
  uint8_t  allow_read_only;
  uint8_t  flood_max;
  uint8_t  interference_threshold;
  uint8_t  agc_reset_interval;    // secs / 4
  uint8_t  bridge_enabled;
  uint16_t bridge_delay;          // milliseconds
  uint8_t  bridge_pkt_src;        // 0=logTx, 1=logRx
  uint32_t bridge_baud;
  uint8_t  bridge_channel;        // 1-14 (ESP-NOW only)
  char     bridge_secret[16];
  uint8_t  powersaving_enabled;
  uint32_t discovery_mod_timestamp;
  float    adc_multiplier;
  char     owner_info[120];
  uint8_t  loop_detect;

  // === Companion-only fields ===
  uint32_t ble_pin;
  uint8_t  telemetry_mode_base;
  uint8_t  telemetry_mode_loc;
  uint8_t  telemetry_mode_env;
  uint8_t  manual_add_contacts;
  uint8_t  buzzer_quiet;
  uint8_t  autoadd_config;
  uint8_t  client_repeat;
  uint8_t  autoadd_max_hops;
  char     default_scope_name[31];
  uint8_t  default_scope_key[16];
};
