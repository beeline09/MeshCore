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
 * Пишет столько, сколько влезает, но всегда считает полную длину — чтобы вызывающий код мог
 * понять, годится ли этот вариант рендера, ещё до того как остановиться на нём.
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

/**
 * Приводит текст к нижнему регистру: ASCII плюс кириллица в UTF-8. Своя реализация нужна
 * потому, что strncasecmp работает побайтово и двухбайтовые буквы ему не по зубам.
 *
 * В UTF-8 заглавные А..П — это D0 90..D0 9F, строчные а..п — D0 B0..D0 BF (тот же ведущий
 * байт, +0x20). А вот Р..Я (D0 A0..D0 AF) переходят в р..я (D1 80..D1 8F), то есть меняется
 * и ведущий байт. Отдельный случай — Ё (D0 81) → ё (D1 91).
 */
void foldLower(const char* src, int len, char* dst, int dst_sz) {
  int o = 0;
  for (int i = 0; i < len; ) {
    uint8_t c = (uint8_t)src[i];

    if (c < 0x80) {
      if (o + 1 > dst_sz - 1) break;
      dst[o++] = (char)((c >= 'A' && c <= 'Z') ? c + 0x20 : c);
      i++;
      continue;
    }

    if (c != 0xD0 && c != 0xD1) {   // не кириллица — копируем как есть
      if (o + 1 > dst_sz - 1) break;
      dst[o++] = (char)c;
      i++;
      continue;
    }

    if (i + 1 >= len || o + 2 > dst_sz - 1) break;
    uint8_t d = (uint8_t)src[i + 1];

    if (c == 0xD0 && d >= 0x90 && d <= 0x9F) {          // А..П
      dst[o++] = (char)0xD0;
      dst[o++] = (char)(d + 0x20);
    } else if (c == 0xD0 && d >= 0xA0 && d <= 0xAF) {   // Р..Я
      dst[o++] = (char)0xD1;
      dst[o++] = (char)(d - 0x20);
    } else if (c == 0xD0 && d == 0x81) {                // Ё
      dst[o++] = (char)0xD1;
      dst[o++] = (char)0x91;
    } else {
      dst[o++] = (char)c;
      dst[o++] = (char)d;
    }
    i += 2;
  }
  dst[o] = 0;
}

/** Сравнение с двумя вариантами написания триггера; пустой вариант просто пропускается. */
bool matchesTrigger(const char* folded, const char* latin, const char* cyrillic) {
  if (latin[0] && strcmp(folded, latin) == 0) return true;
  if (cyrillic[0] && strcmp(folded, cyrillic) == 0) return true;
  return false;
}

void putLineBreak(Appender& a) {
  a.put('\r');
  a.put('\n');
}

/**
 * Упоминание отправителя. Скобки обязательны: клиенты MeshCore (официальное приложение
 * с 1.27, веб-клиенты) распознают именно `@[Имя]` и превращают его в кликабельный бейдж,
 * ведущий в личку. Голое `@Имя` остаётся просто текстом.
 *
 * Официальный ответ — `@[Имя] ` с пробелом после скобки: без него бейдж не кликается.
 * Сразу перевод строки клиент схлопывает, поэтому после пробела ещё U+200B и `\r\n`.
 */
void putMention(Appender& a, const char* sender) {
  a.put("@[");
  a.put(sender);
  a.put("] ");
  a.put("\xE2\x80\x8B");  // U+200B: чтобы пробел/перевод не съелись вместе с упоминанием
  putLineBreak(a);
}

/** Одна запись маршрута: имя, если резолвится, иначе hex. Имя и hex не «все или ничего».
 *  Hex — ровно hash_size байт из path пакета (1/2/3). Это размер, которым отправитель
 *  ping собрал маршрут; у всех хопов в одном пакете он один и тот же.
 */
void putHopLine(Appender& a, MyMesh* mesh, const uint8_t* hop, uint8_t hash_size, bool with_hex,
                bool newline) {
  char hex[2 * 3 + 1];
  toLowerHex(hex, hop, hash_size);

  char name[32];
  if (mesh->lookupRepeaterByHash(hop, hash_size, name, sizeof(name)) == 1) {
    a.put(name);
    if (with_hex) {
      a.put('[');
      a.put(hex);
      a.put(']');
    }
  } else {
    a.put('[');   // без скобок hex не отличить от короткого имени
    a.put(hex);
    a.put(']');
  }
  if (newline) putLineBreak(a);
}

void putHopsTail(Appender& a, uint8_t hop_count) {
  if (hop_count == 0) {   // услышали отправителя напрямую, репитеров между нами нет
    a.put("0 hops - Direct");
    return;
  }
  char tail[24];
  snprintf(tail, sizeof(tail), "%u hops", (unsigned)hop_count);
  a.put(tail);
}

} // namespace

/* ---- Методы MyMesh, относящиеся к роли: держим их здесь, а не в MyMesh.cpp ---- */

// Биты autoadd_config продублированы из MyMesh.cpp, где они объявлены локально и не вынесены
// в заголовок. Если upstream поменяет значения — синхронизировать.
#define PB_AUTO_ADD_OVERWRITE_OLDEST (1 << 0)
#define PB_AUTO_ADD_REPEATER         (1 << 2)

void MyMesh::initPingBot() {
  // ключ хештег-группы — первые 16 байт sha256("#имя")
  ChannelDetails ch;
  memset(&ch, 0, sizeof(ch));
  strcpy(ch.name, PING_BOT_CHANNEL_NAME);
  mesh::Utils::sha256(ch.channel.secret, 16, (const uint8_t*)PING_BOT_CHANNEL_NAME,
                      strlen(PING_BOT_CHANNEL_NAME));

  int idx = -1;
  int free_idx = -1;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails existing;
    if (!getChannel(i, existing)) continue;
    if (existing.name[0] == 0) {
      if (free_idx < 0) free_idx = i;
    } else if (strcmp(existing.name, PING_BOT_CHANNEL_NAME) == 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0 && free_idx >= 0 && setChannel(free_idx, ch)) {
    idx = free_idx;
    saveChannels();
    MESH_DEBUG_PRINTLN("PingBot: канал %s зарегистрирован в слоте %d", PING_BOT_CHANNEL_NAME, idx);
  }
  if (idx < 0) {
    MESH_DEBUG_PRINTLN("PingBot: ОШИБКА — нет свободного слота канала для %s", PING_BOT_CHANNEL_NAME);
    return;
  }

  // в ответе фигурируют только репитеры, поэтому на остальные типы слоты контактов не тратим
  _prefs.manual_add_contacts = 1;
  _prefs.autoadd_config = PB_AUTO_ADD_REPEATER | PB_AUTO_ADD_OVERWRITE_OLDEST;

  // "*" означает отсутствие региона: ответ уходит обычным unscoped-флудом
  memset(&_ping_bot_scope, 0, sizeof(_ping_bot_scope));
  if (strcmp(PING_BOT_REGION, "*") != 0) {
    TransportKeyStore temp;
    temp.getAutoKeyFor(0, "#" PING_BOT_REGION, _ping_bot_scope);
  }

  _ping_bot.begin(this, (uint8_t)idx);
}

void MyMesh::logRx(mesh::Packet* packet, int len, float score) {
  // вызывается на каждый разобранный пакет ДО дедупликации, чтобы бот видел копии с другими маршрутами
  _ping_bot.onRawRx(packet);
}

int MyMesh::lookupRepeaterByHash(const uint8_t* hash, uint8_t hash_len, char* out_name,
                                 size_t out_sz) {
  // 1-байтовый path-хеш часто совпадает у нескольких контактов — из-за этого раньше
  // имя выбрасывалось, даже если репитер в базе есть. Берём наиболее свежий advert,
  // репитера предпочитаем остальным типам.
  ContactInfo best;
  bool have = false;
  int total = getNumContacts();
  for (int i = 0; i < total; i++) {
    ContactInfo contact;
    if (!getContactByIdx(i, contact)) continue;
    if (memcmp(contact.id.pub_key, hash, hash_len) != 0) continue;

    if (!have) {
      best = contact;
      have = true;
      continue;
    }
    bool is_rep = contact.type == ADV_TYPE_REPEATER;
    bool best_rep = best.type == ADV_TYPE_REPEATER;
    if (is_rep && !best_rep) {
      best = contact;
    } else if (is_rep == best_rep && contact.lastmod > best.lastmod) {
      best = contact;
    }
  }
  if (!have) return 0;
  strncpy(out_name, best.name, out_sz - 1);
  out_name[out_sz - 1] = 0;
  return 1;
}

bool MyMesh::sendPingBotReply(uint8_t channel_idx, const char* text) {
  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return false;

  TransportKey saved_scope = send_scope;
  bool saved_unscoped = send_unscoped;

  send_scope = _ping_bot_scope;
  send_unscoped = _ping_bot_scope.isNull();  // без региона: не откатываться на скоуп по умолчанию

  bool ok = sendGroupMessage(getRTCClock()->getCurrentTime(), ch.channel, _prefs.node_name, text,
                             strlen(text));

  send_scope = saved_scope;
  send_unscoped = saved_unscoped;
  return ok;
}

/* ---------------------------------- собственно бот ------------------------------------ */

void PingBot::begin(MyMesh* mesh, uint8_t channel_idx) {
  _mesh = mesh;
  _channel_idx = channel_idx;
  _enabled = true;
  _sender[0] = 0;
  _hour_deadline = millis() + 3600000UL;
}

bool PingBot::onChannelText(uint8_t channel_idx, const mesh::Packet* pkt, const char* text) {
  if (!_enabled || channel_idx != _channel_idx || pkt == NULL || text == NULL) return false;

  // текст группы приходит в виде "<имя отправителя>: <тело>"
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

  char folded[48];
  foldLower(body, body_len, folded, sizeof(folded));

  bool is_ping = matchesTrigger(folded, PING_BOT_TRIGGER, PING_BOT_TRIGGER_RU);
  bool is_pong = matchesTrigger(folded, PING_BOT_PONG_TRIGGER, PING_BOT_PONG_TRIGGER_RU);
  if (!is_ping && !is_pong) return false;

  char sender[32];
  memcpy(sender, text, name_len);
  sender[name_len] = 0;

  if (strcmp(sender, _mesh->getNodeName()) == 0) return true;  // эхо нашего же ответа

  if (is_pong) {   // готовая фраза, маршрут собирать не нужно
    sendPongReply(sender);
    return true;
  }

  if (_window_open) return true;      // уже собираем маршруты для предыдущего пинга
  if (isRateLimited(sender)) return true;

  strcpy(_sender, sender);
  pkt->calculatePacketHash(_trigger_hash);

  _have_best = false;
  captureCandidate(pkt);
  if (!_have_best) {  // пришёл direct-маршрутом: пути нет, считаем это нулём хопов
    _best_hop_count = 0;
    _best_hash_size = 1;
    _have_best = true;
  }

  _window_open = true;
  _window_deadline = millis() + PING_BOT_WINDOW_MS;
  MESH_DEBUG_PRINTLN("PingBot: триггер от '%s', собираем маршруты %d мс", _sender,
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
  if (!pkt->isRouteFlood()) return;  // путь несут только флудовые пакеты

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

int PingBot::availableTextLen() const {
  // sendGroupMessage() сам подставляет префикс "<имя ноды>: " и считает его в тот же лимит
  return PING_BOT_MAX_MSG_LEN - ((int)strlen(_mesh->getNodeName()) + 2);
}

void PingBot::sendPongReply(const char* sender) {
  if (isRateLimited(sender)) return;

  int avail = availableTextLen();

  char buf[PING_BOT_MAX_MSG_LEN + 1];
  Appender a(buf, sizeof(buf));
  putMention(a, sender);
  a.put(PING_BOT_PONG_REPLY);

  const char* text = buf;
  if (a.needed > avail) {   // длинное имя отправителя: отвечаем без упоминания
    text = PING_BOT_PONG_REPLY;
    if ((int)strlen(text) > avail) {
      MESH_DEBUG_PRINTLN("PingBot: имя ноды не оставляет места под ответ на pong");
      return;
    }
  }
  if (_mesh->sendPingBotReply(_channel_idx, text)) {
    noteReply(sender);
  }
}

void PingBot::sendReply() {
  if (!_have_best || _mesh == nullptr) return;

  int avail = availableTextLen();
  if (avail < 8) {
    MESH_DEBUG_PRINTLN("PingBot: имя ноды не оставляет места под ответ");
    return;
  }

  char buf[PING_BOT_MAX_MSG_LEN + 1];
  // имена держим на каждом хопе, где они есть; не скатываемся в голый hex из-за
  // одного неизвестного репитера. Если не влезает — сначала без скобок, потом элизия.
  int needed = renderNamed(buf, sizeof(buf), true, 0);
  if (needed > avail) needed = renderNamed(buf, sizeof(buf), false, 0);
  for (int elide = 1; needed > avail && elide <= _best_hop_count; elide++) {
    needed = renderNamed(buf, sizeof(buf), false, elide);
  }
  for (int elide = 0; needed > avail && elide <= _best_hop_count; elide++) {
    needed = renderCompact(buf, sizeof(buf), elide);
  }
  if (needed > avail) return;  // не отправляем маршрут, у которого срезан хвост 'N hops'

  if (_mesh->sendPingBotReply(_channel_idx, buf)) {
    noteReply(_sender);
  }
}

int PingBot::renderNamed(char* out, int out_sz, bool with_hex, int elide) const {
  Appender a(out, out_sz);
  putMention(a, _sender);

  int total = _best_hop_count;
  // path[0] — репитер, услышавший отправителя первым, поэтому печатаем его последним
  if (total > 0 && elide < total) {
    int shown = total - elide;
    int head = (shown + 1) / 2;
    int tail_count = shown - head;

    for (int k = 0; k < head; k++) {
      int i = total - 1 - k;
      putHopLine(a, _mesh, &_best_path[i * _best_hash_size], _best_hash_size, with_hex, true);
    }
    if (elide > 0) {
      a.put("...");
      putLineBreak(a);
    }
    for (int k = tail_count; k > 0; k--) {
      int i = k - 1;
      putHopLine(a, _mesh, &_best_path[i * _best_hash_size], _best_hash_size, with_hex, true);
    }
  } else if (elide > 0 && total > 0) {
    a.put("...");
    putLineBreak(a);
  }

  putHopsTail(a, _best_hop_count);
  return a.needed;
}

int PingBot::renderCompact(char* out, int out_sz, int elide) const {
  Appender a(out, out_sz);
  putMention(a, _sender);

  int total = _best_hop_count;
  if (total > 0 && elide < total) {
    int shown = total - elide;
    int head = (shown + 1) / 2;
    int tail_count = shown - head;
    bool first = true;

    for (int k = 0; k < head; k++) {
      if (!first) a.put(", ");
      first = false;
      putHopLine(a, _mesh, &_best_path[(total - 1 - k) * _best_hash_size], _best_hash_size,
                 false, false);
    }
    if (elide > 0) {
      if (!first) a.put(", ");
      first = false;
      a.put("...");
    }
    for (int k = tail_count; k > 0; k--) {
      if (!first) a.put(", ");
      first = false;
      putHopLine(a, _mesh, &_best_path[(k - 1) * _best_hash_size], _best_hash_size, false, false);
    }
    putLineBreak(a);
  }

  putHopsTail(a, (uint8_t)total);
  return a.needed;
}

#endif
