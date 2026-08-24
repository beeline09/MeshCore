#include "PingBot.h"

#ifdef WITH_PING_BOT

#include "MyMesh.h"

namespace {

bool hasPassed(unsigned long deadline) {
  return (long)(millis() - deadline) >= 0;
}

void toLowerHex(char* dest, const uint8_t* src, uint8_t len) {
  static const char digits[] = "0123456789abcdef";
  while (len-- > 0) {
    uint8_t b = *src++;
    *dest++ = digits[b >> 4];
    *dest++ = digits[b & 0x0F];
  }
  *dest = 0;
}

/**
 * Writes as much as fits while always counting the full length, so a caller can decide
 * whether a rendering is usable before committing to it.
 */
struct Appender {
  char* buf;
  int cap, len, needed;

  Appender(char* b, int c) : buf(b), cap(c), len(0), needed(0) {
    if (cap > 0) buf[0] = 0;
  }
  void put(char c) {
    needed++;
    if (len < cap - 1) {
      buf[len++] = c;
      buf[len] = 0;
    }
  }
  void put(const char* s) {
    while (*s) put(*s++);
  }
};

} // namespace

void PingBot::begin(MyMesh* mesh, uint8_t channel_idx) {
  _mesh = mesh;
  _channel_idx = channel_idx;
  _enabled = true;
  _sender[0] = 0;
  _hour_deadline = millis() + 3600000UL;
}

bool PingBot::onChannelText(uint8_t channel_idx, const mesh::Packet* pkt, const char* text) {
  if (!_enabled || channel_idx != _channel_idx || pkt == NULL || text == NULL) return false;

  // group text arrives as "<sender name>: <body>"
  const char* sep = strstr(text, ": ");
  if (sep == NULL) return false;

  int name_len = sep - text;
  if (name_len < 1 || name_len > 31) return false;

  const char* body = sep + 2;
  while (*body == ' ') body++;
  int body_len = strlen(body);
  while (body_len > 0 && (body[body_len - 1] == ' ' || body[body_len - 1] == '\r' ||
                          body[body_len - 1] == '\n')) {
    body_len--;
  }

  const int trigger_len = sizeof(PING_BOT_TRIGGER) - 1;
  if (body_len != trigger_len || strncasecmp(body, PING_BOT_TRIGGER, trigger_len) != 0) return false;

  char sender[32];
  memcpy(sender, text, name_len);
  sender[name_len] = 0;

  if (strcmp(sender, _mesh->getNodeName()) == 0) return true;  // our own reply echoed back

  if (_window_open) return true;      // already collecting routes for an earlier ping
  if (isRateLimited(sender)) return true;

  strcpy(_sender, sender);
  pkt->calculatePacketHash(_trigger_hash);

  _have_best = false;
  captureCandidate(pkt);
  if (!_have_best) {  // arrived direct-routed: no usable path, report as zero hops
    _best_hop_count = 0;
    _best_hash_size = 1;
    _have_best = true;
  }

  _window_open = true;
  _window_deadline = millis() + PING_BOT_WINDOW_MS;
  MESH_DEBUG_PRINTLN("PingBot: trigger from '%s', collecting for %d ms", _sender,
                     (int)PING_BOT_WINDOW_MS);
  return true;
}

void PingBot::onRawRx(const mesh::Packet* pkt) {
  if (!_window_open || pkt == NULL) return;
  if (pkt->getPayloadType() != PAYLOAD_TYPE_GRP_TXT) return;

  uint8_t hash[MAX_HASH_SIZE];
  pkt->calculatePacketHash(hash);
  if (memcmp(hash, _trigger_hash, MAX_HASH_SIZE) != 0) return;

  captureCandidate(pkt);
}

void PingBot::loop() {
  if (!_enabled) return;

  if (_window_open && hasPassed(_window_deadline)) {
    _window_open = false;
    sendReply();
  }
  if (hasPassed(_hour_deadline)) {
    _hour_deadline = millis() + 3600000UL;
    _replies_this_hour = 0;
  }
}

void PingBot::captureCandidate(const mesh::Packet* pkt) {
  if (!pkt->isRouteFlood()) return;  // only flood packets carry a path

  uint8_t hop_count = pkt->getPathHashCount();
  if (_have_best && hop_count >= _best_hop_count) return;

  uint8_t hash_size = pkt->getPathHashSize();
  if (hop_count * hash_size > MAX_PATH_SIZE) return;

  memcpy(_best_path, pkt->path, hop_count * hash_size);
  _best_hop_count = hop_count;
  _best_hash_size = hash_size;
  _have_best = true;
}

bool PingBot::isRateLimited(const char* sender) {
  if (_replies_this_hour >= PING_BOT_MAX_PER_HOUR) return true;

  for (int i = 0; i < PING_BOT_COOLDOWN_SLOTS; i++) {
    if (_cooldowns[i].name[0] && strcmp(_cooldowns[i].name, sender) == 0) {
      return !hasPassed(_cooldowns[i].until);
    }
  }
  return false;
}

void PingBot::noteReply(const char* sender) {
  _replies_this_hour++;
  unsigned long until = millis() + (unsigned long)PING_BOT_COOLDOWN_SEC * 1000UL;

  for (int i = 0; i < PING_BOT_COOLDOWN_SLOTS; i++) {
    if (_cooldowns[i].name[0] && strcmp(_cooldowns[i].name, sender) == 0) {
      _cooldowns[i].until = until;
      return;
    }
  }
  Cooldown* slot = &_cooldowns[_next_cooldown];
  _next_cooldown = (_next_cooldown + 1) % PING_BOT_COOLDOWN_SLOTS;
  strncpy(slot->name, sender, sizeof(slot->name) - 1);
  slot->name[sizeof(slot->name) - 1] = 0;
  slot->until = until;
}

void PingBot::sendReply() {
  if (!_have_best || _mesh == nullptr) return;

  // sendGroupMessage() prepends "<node name>: " and counts it against the same limit
  int avail = PING_BOT_MAX_MSG_LEN - ((int)strlen(_mesh->getNodeName()) + 2);
  if (avail < 8) {
    MESH_DEBUG_PRINTLN("PingBot: node name leaves no room for a reply");
    return;
  }

  char buf[PING_BOT_MAX_MSG_LEN + 1];
  int needed = renderNamed(buf, sizeof(buf), true);
  if (needed > avail) needed = renderNamed(buf, sizeof(buf), false);
  for (int elide = 0; needed > avail && elide <= _best_hop_count; elide++) {
    needed = renderCompact(buf, sizeof(buf), elide);
  }
  if (needed > avail) return;  // never send a route that lost its '-N hops' tail

  if (_mesh->sendPingBotReply(_channel_idx, buf)) {
    noteReply(_sender);
  }
}

int PingBot::renderNamed(char* out, int out_sz, bool with_hex) const {
  Appender a(out, out_sz);
  a.put('@');
  a.put(_sender);
  a.put('\n');

  // path[0] is the repeater that heard the sender first, so print it last
  for (int i = (int)_best_hop_count - 1; i >= 0; i--) {
    const uint8_t* hop = &_best_path[i * _best_hash_size];
    char hex[2 * 3 + 1];
    toLowerHex(hex, hop, _best_hash_size);

    char name[32];
    if (_mesh->lookupRepeaterByHash(hop, _best_hash_size, name, sizeof(name)) == 1) {
      a.put(name);
      if (with_hex) {
        a.put('[');
        a.put(hex);
        a.put(']');
      }
    } else {
      a.put(hex);  // unknown or ambiguous prefix
    }
    a.put('\n');
  }

  char tail[24];
  snprintf(tail, sizeof(tail), "-%u hops", (unsigned)_best_hop_count);
  a.put(tail);
  return a.needed;
}

int PingBot::renderCompact(char* out, int out_sz, int elide) const {
  Appender a(out, out_sz);
  a.put('@');
  a.put(_sender);
  a.put('\n');

  int total = _best_hop_count;
  if (total > 0 && elide < total) {
    int shown = total - elide;
    int head = (shown + 1) / 2;
    int tail_count = shown - head;
    bool first = true;

    for (int k = 0; k < head; k++) {
      char hex[2 * 3 + 1];
      toLowerHex(hex, &_best_path[(total - 1 - k) * _best_hash_size], _best_hash_size);
      if (!first) a.put(", ");
      first = false;
      a.put(hex);
    }
    if (elide > 0) {
      if (!first) a.put(", ");
      first = false;
      a.put("...");
    }
    for (int k = tail_count; k > 0; k--) {
      char hex[2 * 3 + 1];
      toLowerHex(hex, &_best_path[(k - 1) * _best_hash_size], _best_hash_size);
      if (!first) a.put(", ");
      first = false;
      a.put(hex);
    }
    a.put('\n');
  }

  char tail[24];
  snprintf(tail, sizeof(tail), "-%u hops", (unsigned)total);
  a.put(tail);
  return a.needed;
}

#endif
