#pragma once
// Доступ к настройкам Darktec `/darktec.prefs` во внутренней ФС (spec 0001,
// FR-14..19, D-7). Та же InternalFS (LittleFS), где ядро хранит свои prefs;
// структура NodePrefs ядра не затрагивается. На native-платформе доступен
// только чистый формат (DarktecPrefsFormat.h).

#include "DarktecPrefsFormat.h"

#ifdef NRF52_PLATFORM

namespace DarktecPrefs {

using Settings = DarktecPrefsFormat::Settings;

// Ленивая, идемпотентная инициализация: сам вызывает InternalFS.begin()
// (безопасно при повторе — ядро монтирует ФС позже, в main()). Вызывается
// из DarktecBoard::begin(); можно звать в любой момент.
void begin();

// Текущие значения (всегда валидные: при недоступной ФС — дефолты флагов,
// загрузка не блокируется — FR-15/D-7).
const Settings& get();

// Атомарная запись: /darktec.tmp → rename (FR-19). Значения проходят
// sanitize. При ошибке ФС возвращает false, in-memory остаётся прежним.
bool save(const Settings& s);

}  // namespace DarktecPrefs

#endif  // NRF52_PLATFORM
