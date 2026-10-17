// DarktecPrefs: файл настроек /darktec.prefs в InternalFS (spec 0001).
// См. DarktecPrefsFormat.h — чистый формат; здесь только доступ к ФС (NRF52).

#include "DarktecPrefs.h"

#ifdef NRF52_PLATFORM

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

namespace {

using namespace DarktecPrefsFormat;

constexpr const char* kPath = "/darktec.prefs";
constexpr const char* kTmpPath = "/darktec.tmp";

Settings current;
bool loaded = false;

// Ленивая загрузка (FR-15, D-7): сами монтируем ФС (Adafruit InternalFS
// допускает повторный begin — ядро в main() вызовет его ещё раз, это no-op).
// При любой ошибке остаёмся на дефолтах флагов; повторов чтения нет —
// следующий шанс прочитать файл будет после save()/перезагрузки.
void ensureLoaded() {
  if (loaded) return;
  loaded = true;

  Settings s = buildFlagDefaults();
  InternalFS.begin();
  Adafruit_LittleFS_Namespace::File f = InternalFS.open(kPath, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (f) {
    uint8_t buf[kWireSize];
    const int n = f.read(buf, sizeof(buf));
    f.close();
    Settings parsed{};
    if (n == (int)kWireSize && parse(buf, (size_t)n, parsed)) {
      s = parsed;
    }
  }
  current = s;
}

}  // namespace

namespace DarktecPrefs {

void begin() { ensureLoaded(); }

const Settings& get() {
  ensureLoaded();
  return current;
}

bool save(const Settings& in) {
  ensureLoaded();
  Settings s = in;
  sanitize(s);

  uint8_t buf[kWireSize];
  const size_t n = serialize(s, buf, sizeof(buf));
  if (n != kWireSize) return false;

  Adafruit_LittleFS_Namespace::File f = InternalFS.open(kTmpPath, Adafruit_LittleFS_Namespace::FILE_O_WRITE);
  if (!f) return false;
  const int written = f.write(buf, n);
  f.close();
  if (written != (int)n) {
    InternalFS.remove(kTmpPath);
    return false;
  }

  // LittleFS rename атомарен (FR-19); цель убираем на случай повторной записи.
  InternalFS.remove(kPath);
  const bool ok = InternalFS.rename(kTmpPath, kPath);
  if (ok) current = s;
  return ok;
}

}  // namespace DarktecPrefs

#endif  // NRF52_PLATFORM
