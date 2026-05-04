#pragma once
#include <stdint.h>
#include <Mesh.h>
#include <helpers/SensorManager.h>
#include <RTClib.h>

#define TS_MIN_VALID       1577836800u  // 2020-01-01 UTC
#define TS_MAX_VALID       2524608000u  // 2050-01-01 UTC
#define TS_CLUSTER_WINDOW  60u
#define TS_DRIFT_THRESHOLD 120u
#define TS_MAX_JUMP        36000u

enum class TsSyncResult : uint8_t { NONE, CLOCK, GPS };

class TimeSyncHelper {
public:
  struct Sample { uint32_t ts; uint32_t pub_hash; };

  Sample   _buf[10] = {};
  int      _buf_pos = 0;
  int      _buf_count = 0;
  uint32_t _sync_count = 0;
  uint32_t _advert_count = 0;
  uint32_t _valid_count = 0;
  uint32_t _last_sync = 0;
  int32_t  _last_adj = 0;
  int      _best_cluster = 0;

  // Runtime-tunable parameters (reset to defaults on reboot)
  uint32_t cluster_window  = TS_CLUSTER_WINDOW;
  uint32_t drift_threshold = TS_DRIFT_THRESHOLD;
  uint32_t max_jump        = TS_MAX_JUMP;

  // Feed advert timestamp (increments _advert_count).
  // Returns true if added to buffer.
  bool feedAdvert(uint32_t ts, uint32_t pub_hash);

  // Feed message timestamp (no advert counting, MAX_JUMP guard after first sync).
  // cur_time: current RTC time for the jump check.
  bool feedMessage(uint32_t ts, uint32_t cur_time);

  // Run quorum algorithm and apply sync if quorum reached.
  // app_time_locked: caller passes true during CMD_SET_DEVICE_TIME holdoff.
  // Returns NONE/CLOCK/GPS.
  TsSyncResult trySync(mesh::RTCClock* rtc, SensorManager* sensors, bool app_time_locked = false);

  // Reset sync state, preserve runtime params.
  void reset() {
    memset(_buf, 0, sizeof(_buf));
    _buf_pos = _buf_count = _sync_count = _advert_count = _valid_count = 0;
    _last_sync = 0; _last_adj = 0; _best_cluster = 0;
  }

  // Build timesync status reply into buf (caller ensures >= 320 bytes).
  void buildReply(char* buf, mesh::RTCClock* rtc) const;
};
