#pragma once

#ifdef WITH_PING_BOT

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>

/* ------------------------- role configuration ------------------------- */

#ifndef PING_BOT_NAME
  #define PING_BOT_NAME "PingBot"
#endif

// hashtag group name, WITHOUT the leading '#'
#ifndef PING_BOT_GROUP
  #define PING_BOT_GROUP "ping"
#endif

#ifndef PING_BOT_TRIGGER
  #define PING_BOT_TRIGGER "ping"
#endif

// transport scope for replies, WITHOUT the leading '#'. "*" means no region at all,
// i.e. plain un-scoped flood.
#ifndef PING_BOT_REGION
  #define PING_BOT_REGION "*"
#endif

// full reply size, INCLUDING the "<node name>: " prefix that sendGroupMessage() prepends
#ifndef PING_BOT_MAX_MSG_LEN
  #define PING_BOT_MAX_MSG_LEN 155
#endif

// how long to keep collecting duplicate copies of the same packet before replying
#ifndef PING_BOT_WINDOW_MS
  #define PING_BOT_WINDOW_MS 8000
#endif

#ifndef PING_BOT_COOLDOWN_SEC
  #define PING_BOT_COOLDOWN_SEC 300
#endif

#ifndef PING_BOT_MAX_PER_HOUR
  #define PING_BOT_MAX_PER_HOUR 30
#endif

/* --------------------- compile-time sanity checks --------------------- */

#if PING_BOT_MAX_MSG_LEN > MAX_TEXT_LEN
  #error "PING_BOT_MAX_MSG_LEN exceeds MAX_TEXT_LEN (160). sendGroupMessage() would silently truncate the reply and cut off the trailing '-N hops' line. Lower PING_BOT_MAX_MSG_LEN to 160 or less."
#endif

#if PING_BOT_MAX_MSG_LEN < 48
  #error "PING_BOT_MAX_MSG_LEN is too small to hold even '@<sender>' plus '-N hops'. Use 48 or more."
#endif

#if PING_BOT_WINDOW_MS < 1000 || PING_BOT_WINDOW_MS > 60000
  #error "PING_BOT_WINDOW_MS must be between 1000 and 60000 ms."
#endif

static_assert(sizeof(PING_BOT_NAME) - 1 >= 1,
              "PING_BOT_NAME must not be empty.");
static_assert(sizeof(PING_BOT_NAME) - 1 <= 31,
              "PING_BOT_NAME must be 31 chars or less (NodePrefs::node_name is char[32]).");
static_assert(sizeof(PING_BOT_GROUP) - 1 >= 1,
              "PING_BOT_GROUP must not be empty.");
static_assert(sizeof(PING_BOT_GROUP) - 1 <= 30,
              "PING_BOT_GROUP must be 30 chars or less (channel name is char[32] and gets a '#' prefix).");
static_assert(PING_BOT_GROUP[0] != '#',
              "PING_BOT_GROUP must be given WITHOUT the leading '#', it is added by the firmware.");
static_assert(sizeof(PING_BOT_TRIGGER) - 1 >= 1,
              "PING_BOT_TRIGGER must not be empty.");
static_assert(sizeof(PING_BOT_REGION) - 1 >= 1,
              "PING_BOT_REGION must not be empty. Use \"*\" for no region.");
static_assert(sizeof(PING_BOT_REGION) - 1 <= 30,
              "PING_BOT_REGION must be 30 chars or less (it gets a '#' prefix before hashing).");
static_assert(PING_BOT_REGION[0] != '#',
              "PING_BOT_REGION must be given WITHOUT the leading '#', it is added by the firmware.");

/* ---------------------------------------------------------------------- */

#define PING_BOT_CHANNEL_NAME  "#" PING_BOT_GROUP
#define PING_BOT_COOLDOWN_SLOTS 8

class MyMesh;

/**
 * Listens on a hashtag group and answers "ping" with the route the packet took to get here.
 *
 * Duplicate copies of a flood packet are deduplicated by Mesh::onRecvPacket() before the
 * channel handler sees them, and the packet hash does not cover 'path' — so the copy that
 * reaches onChannelText() is merely the first to arrive, not the one with fewest hops.
 * To find the shortest route we keep a collection window open and let onRawRx(), driven by
 * the pre-dedup logRx() hook, offer the remaining copies.
 */
class PingBot {
public:
  void begin(MyMesh* mesh, uint8_t channel_idx);

  bool isEnabled() const { return _enabled; }
  uint8_t getChannelIdx() const { return _channel_idx; }

  /** Returns true when the text was a ping trigger and the bot took ownership of it. */
  bool onChannelText(uint8_t channel_idx, const mesh::Packet* pkt, const char* text);

  /** Fed from logRx(), i.e. every received packet, before deduplication. */
  void onRawRx(const mesh::Packet* pkt);

  void loop();

private:
  bool isRateLimited(const char* sender);
  void noteReply(const char* sender);
  void captureCandidate(const mesh::Packet* pkt);
  void sendReply();

  int renderNamed(char* out, int out_sz, bool with_hex) const;
  int renderCompact(char* out, int out_sz, int elide) const;

  MyMesh*  _mesh = nullptr;
  bool     _enabled = false;
  uint8_t  _channel_idx = 0;

  bool          _window_open = false;
  unsigned long _window_deadline = 0;
  uint8_t       _trigger_hash[MAX_HASH_SIZE];
  char          _sender[32];

  bool     _have_best = false;
  uint8_t  _best_path[MAX_PATH_SIZE];
  uint8_t  _best_hop_count = 0;
  uint8_t  _best_hash_size = 1;

  struct Cooldown {
    char          name[32];
    unsigned long until;
  };
  Cooldown _cooldowns[PING_BOT_COOLDOWN_SLOTS] = {};
  int      _next_cooldown = 0;

  unsigned long _hour_deadline = 0;
  uint16_t      _replies_this_hour = 0;
};

#endif
