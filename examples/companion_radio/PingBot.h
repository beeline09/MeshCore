#pragma once

#ifdef WITH_PING_BOT

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>

/* --------------------------- настройки роли --------------------------- */

#ifndef PING_BOT_NAME
  #define PING_BOT_NAME "PingBot"
#endif

// имя хештег-группы, БЕЗ ведущего '#'
#ifndef PING_BOT_GROUP
  #define PING_BOT_GROUP "ping"
#endif

// Слова-триггеры задаются В НИЖНЕМ РЕГИСТРЕ: входящий текст приводится к нижнему регистру
// перед сравнением (ASCII и кириллица), поэтому писать можно как угодно — ping, PING, ПиНг.
// Пустая строка "" отключает соответствующий вариант.
#ifndef PING_BOT_TRIGGER
  #define PING_BOT_TRIGGER "ping"
#endif
#ifndef PING_BOT_TRIGGER_RU
  #define PING_BOT_TRIGGER_RU "пинг"
#endif

// На "pong"/"понг" бот отвечает готовой фразой, без сбора маршрута
#ifndef PING_BOT_PONG_TRIGGER
  #define PING_BOT_PONG_TRIGGER "pong"
#endif
#ifndef PING_BOT_PONG_TRIGGER_RU
  #define PING_BOT_PONG_TRIGGER_RU "понг"
#endif
#ifndef PING_BOT_PONG_REPLY
  #define PING_BOT_PONG_REPLY "Ты мне ping, я тебе - pong)"
#endif

// регион (transport scope) для ответа, БЕЗ ведущего '#'. "*" означает отсутствие
// региона, то есть обычный unscoped-флуд.
#ifndef PING_BOT_REGION
  #define PING_BOT_REGION "*"
#endif

// полный размер ответа, ВКЛЮЧАЯ префикс "<имя ноды>: ", который подставляет sendGroupMessage()
#ifndef PING_BOT_MAX_MSG_LEN
  #define PING_BOT_MAX_MSG_LEN 155
#endif

// сколько собирать копии одного и того же пакета, пришедшие разными маршрутами, до ответа
#ifndef PING_BOT_WINDOW_MS
  #define PING_BOT_WINDOW_MS 8000
#endif

#ifndef PING_BOT_COOLDOWN_SEC
  #define PING_BOT_COOLDOWN_SEC 300
#endif

#ifndef PING_BOT_MAX_PER_HOUR
  #define PING_BOT_MAX_PER_HOUR 30
#endif

// сколько отправителей одновременно помнит кулдаун. При переполнении вытесняется самая
// старая запись, и тот отправитель пройдёт раньше срока. Около 36 байт RAM на слот.
#ifndef PING_BOT_COOLDOWN_SLOTS
  #define PING_BOT_COOLDOWN_SLOTS 32
#endif

/* --------------------- проверки на этапе компиляции -------------------- */

#if PING_BOT_MAX_MSG_LEN > MAX_TEXT_LEN
  #error "PING_BOT_MAX_MSG_LEN больше MAX_TEXT_LEN (160). sendGroupMessage() молча обрежет ответ и срежет хвост '-N hops'. Уменьшите PING_BOT_MAX_MSG_LEN до 160 или меньше."
#endif

#if PING_BOT_MAX_MSG_LEN < 48
  #error "PING_BOT_MAX_MSG_LEN слишком мал: не вмещает даже '@<отправитель>' и '-N hops'. Укажите 48 или больше."
#endif

#if PING_BOT_WINDOW_MS < 1000 || PING_BOT_WINDOW_MS > 60000
  #error "PING_BOT_WINDOW_MS должен быть от 1000 до 60000 мс."
#endif

#if PING_BOT_COOLDOWN_SEC < 0 || PING_BOT_COOLDOWN_SEC > 86400
  #error "PING_BOT_COOLDOWN_SEC должен быть от 0 (без кулдауна) до 86400 (сутки)."
#endif

#if PING_BOT_MAX_PER_HOUR < 1 || PING_BOT_MAX_PER_HOUR > 65535
  #error "PING_BOT_MAX_PER_HOUR должен быть от 1 до 65535 (счётчик — uint16_t)."
#endif

#if PING_BOT_COOLDOWN_SLOTS < 1 || PING_BOT_COOLDOWN_SLOTS > 256
  #error "PING_BOT_COOLDOWN_SLOTS должен быть от 1 до 256 (каждый слот стоит около 36 байт RAM)."
#endif

static_assert(sizeof(PING_BOT_NAME) - 1 >= 1,
              "PING_BOT_NAME не может быть пустым.");
static_assert(sizeof(PING_BOT_NAME) - 1 <= 31,
              "PING_BOT_NAME не длиннее 31 символа (NodePrefs::node_name — char[32]).");
static_assert(sizeof(PING_BOT_GROUP) - 1 >= 1,
              "PING_BOT_GROUP не может быть пустым.");
static_assert(sizeof(PING_BOT_GROUP) - 1 <= 30,
              "PING_BOT_GROUP не длиннее 30 символов (имя канала — char[32] плюс префикс '#').");
static_assert(PING_BOT_GROUP[0] != '#',
              "PING_BOT_GROUP задаётся БЕЗ ведущего '#', его добавляет прошивка.");
static_assert(sizeof(PING_BOT_TRIGGER) - 1 >= 1 || sizeof(PING_BOT_TRIGGER_RU) - 1 >= 1,
              "Хотя бы один из PING_BOT_TRIGGER / PING_BOT_TRIGGER_RU должен быть непустым.");
static_assert(sizeof(PING_BOT_PONG_REPLY) - 1 <= PING_BOT_MAX_MSG_LEN - 3,
              "PING_BOT_PONG_REPLY не влезает в PING_BOT_MAX_MSG_LEN вместе с префиксом имени ноды.");
static_assert(sizeof(PING_BOT_REGION) - 1 >= 1,
              "PING_BOT_REGION не может быть пустым. Для отсутствия региона укажите \"*\".");
static_assert(sizeof(PING_BOT_REGION) - 1 <= 30,
              "PING_BOT_REGION не длиннее 30 символов (перед хешированием добавляется префикс '#').");
static_assert(PING_BOT_REGION[0] != '#',
              "PING_BOT_REGION задаётся БЕЗ ведущего '#', его добавляет прошивка.");

/* ---------------------------------------------------------------------- */

#define PING_BOT_CHANNEL_NAME  "#" PING_BOT_GROUP

class MyMesh;

/**
 * Слушает хештег-группу и отвечает на "ping" маршрутом, которым пакет дошёл до этой ноды.
 *
 * Копии флудового пакета отсекаются в Mesh::onRecvPacket() ещё до обработчика канала, а хеш
 * пакета не покрывает path — поэтому до onChannelText() доезжает просто первая по времени
 * копия, а не самая короткая. Чтобы найти кратчайший маршрут, держим открытым окно сбора и
 * добираем остальные копии через onRawRx(), который вызывается из хука logRx() до дедупликации.
 */
class PingBot {
public:
  void begin(MyMesh* mesh, uint8_t channel_idx);

  bool isEnabled() const { return _enabled; }
  uint8_t getChannelIdx() const { return _channel_idx; }

  /** true, если текст оказался триггером и бот забрал его себе. */
  bool onChannelText(uint8_t channel_idx, const mesh::Packet* pkt, const char* text);

  /** Вызывается из logRx(), то есть на каждый принятый пакет, до дедупликации. */
  void onRawRx(const mesh::Packet* pkt);

  void loop();

private:
  bool isRateLimited(const char* sender);
  void noteReply(const char* sender);
  void captureCandidate(const mesh::Packet* pkt);
  void sendReply();
  void sendPongReply(const char* sender);

  /** Сколько байт остаётся под текст после префикса "<имя ноды>: ". */
  int availableTextLen() const;

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
