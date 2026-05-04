#include "TimeSyncHelper.h"
#include <RTClib.h>
#include <helpers/sensors/LocationProvider.h>

bool TimeSyncHelper::feedAdvert(uint32_t ts, uint32_t pub_hash) {
  _advert_count++;
  if (ts <= TS_MIN_VALID || ts >= TS_MAX_VALID) return false;
  _valid_count++;
  _buf[_buf_pos] = { ts, pub_hash };
  _buf_pos = (_buf_pos + 1) % 10;
  if (_buf_count < 10) _buf_count++;
  return true;
}

bool TimeSyncHelper::feedMessage(uint32_t ts, uint32_t cur_time) {
  if (ts <= TS_MIN_VALID || ts >= TS_MAX_VALID) return false;
  if (_sync_count > 0) {
    uint32_t diff = (ts > cur_time) ? ts - cur_time : cur_time - ts;
    if (diff >= max_jump) return false;
  }
  _buf[_buf_pos] = { ts, 0 };
  _buf_pos = (_buf_pos + 1) % 10;
  if (_buf_count < 10) _buf_count++;
  return true;
}

TsSyncResult TimeSyncHelper::trySync(mesh::RTCClock* rtc, SensorManager* sensors, bool app_time_locked) {
  if (app_time_locked) return TsSyncResult::NONE;

  bool unset = !rtc->isTimeReliable() && (_sync_count == 0);
  int n      = unset ? (_buf_count < 5  ? _buf_count : 5)  : (_buf_count < 10 ? _buf_count : 10);
  int quorum = unset ? 3 : 7;
  if (_buf_count < quorum) return TsSyncResult::NONE;

  // copy last n samples from ring buffer
  uint32_t tmp[10];
  for (int i = 0; i < n; i++) {
    int idx = (_buf_pos - 1 - i + 10) % 10;
    tmp[i] = _buf[idx].ts;
  }
  // insertion sort
  for (int i = 1; i < n; i++) {
    uint32_t key = tmp[i]; int j = i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j+1] = tmp[j]; j--; }
    tmp[j+1] = key;
  }
  // find densest cluster
  int best_count = 0, best_start = 0;
  for (int i = 0; i < n; i++) {
    int cnt = 0;
    for (int j = i; j < n && tmp[j] - tmp[i] <= cluster_window; j++) cnt++;
    if (cnt > best_count) { best_count = cnt; best_start = i; }
  }
  if (best_count > _best_cluster) _best_cluster = best_count;
  if (best_count < quorum) return TsSyncResult::NONE;

  // median of cluster
  int cluster_end = best_start;
  while (cluster_end < n && tmp[cluster_end] - tmp[best_start] <= cluster_window) cluster_end++;
  uint32_t median = tmp[(best_start + cluster_end) / 2];
  uint32_t current = rtc->getCurrentTime();
  int32_t adj = (int32_t)median - (int32_t)current;

  bool should_apply;
  if (unset) {
    should_apply = true;
  } else {
    uint32_t diff = (median > current) ? median - current : current - median;
    should_apply = (diff > drift_threshold && diff < max_jump);
  }
  if (!should_apply) return TsSyncResult::NONE;

  _last_adj  = adj;
  _last_sync = median;
  _sync_count++;

  // prefer GPS re-sync if available
  if (sensors) {
    LocationProvider* gps = sensors->getLocationProvider();
    if (gps && gps->isEnabled() && gps->isValid()) {
      gps->syncTime();
      MESH_DEBUG_PRINTLN("TimeSync: GPS re-sync (quorum drift %ld s)", (long)adj);
      return TsSyncResult::GPS;
    }
  }
  rtc->setCurrentTime(median);
  MESH_DEBUG_PRINTLN("TimeSync: clock adj %ld s (quorum %d/%d)", (long)adj, best_count, n);
  return TsSyncResult::CLOCK;
}

void TimeSyncHelper::buildReply(char* buf, mesh::RTCClock* rtc) const {
  uint32_t now = rtc->getCurrentTime();
  DateTime dt(now);
  if (_sync_count == 0) {
    bool unset_mode = !rtc->isTimeReliable();
    int n = _buf_count < (unset_mode ? 5 : 10) ? _buf_count : (unset_mode ? 5 : 10);
    sprintf(buf,
      "TimeSync: no sync yet\nAdverts: %lu rx / %lu valid\nBuf: %d/10 (best cluster: %d/%d need %d)\nClock: %02d:%02d:%02d %d-%02d-%02d UTC",
      (unsigned long)_advert_count, (unsigned long)_valid_count,
      _buf_count, _best_cluster, n, unset_mode ? 3 : 7,
      dt.hour(), dt.minute(), dt.second(), dt.year(), dt.month(), dt.day());
  } else {
    uint32_t ago = now > _last_sync ? now - _last_sync : 0;
    DateTime ls(_last_sync);
    sprintf(buf,
      "TimeSync: %lu syncs\nLast: %02d:%02d %d-%02d-%02d UTC (%lus ago)\nAdj: %+lds\nAdverts: %lu rx / %lu valid\nBuf: %d/10\nClock: %02d:%02d:%02d %d-%02d-%02d UTC",
      (unsigned long)_sync_count,
      ls.hour(), ls.minute(), ls.year(), ls.month(), ls.day(),
      (unsigned long)ago, (long)_last_adj,
      (unsigned long)_advert_count, (unsigned long)_valid_count,
      _buf_count,
      dt.hour(), dt.minute(), dt.second(), dt.year(), dt.month(), dt.day());
  }
}
