// DarktecPrefsUI: общая механика правки /darktec.prefs для всех UI Darktec
// (клиент/репитер). Раньше этот код копировался в каждый UITask — теперь
// один header-only источник: черновик в памяти с отложенной флеш-записью,
// логика изменения значений и форматирование строк редактора.
// Включается только в darktec-сборках (рядом с DarktecPrefs.h).

#ifndef DARKTEC_PREFS_UI_H
#define DARKTEC_PREFS_UI_H

#include <Arduino.h>
#include "DarktecPrefs.h"

namespace DarktecPrefsUI {

// Редактор: 6 параметров (i == 0..5) + «Back» последней строкой.
constexpr int kRowBack = 6;

// ── Черновик правок (spec 0002) ──────────────────────────────────────────────
// Клик меняет черновик в памяти мгновенно; флеш-запись — через 1.2 с после
// последней правки (loopTick) или при выходе из правки значения / «Back»
// (flush). Правок не было — флеш не пишется вовсе.

inline DarktecPrefs::Settings& draftVal() {
  static DarktecPrefs::Settings s;
  return s;
}
inline bool& draftValid() { static bool v = false; return v; }
inline bool& draftDirty() { static bool d = false; return d; }
inline unsigned long& draftSaveAt() { static unsigned long t = 0; return t; }

// Что показывать в редакторе: черновик, пока есть несохранённые правки.
inline const DarktecPrefs::Settings& shown() {
  return draftValid() ? draftVal() : DarktecPrefs::get();
}

// Записать черновик во флеш (если есть что писать). При сбое ФС черновик
// остаётся валидным — на экране правленое значение; повтор по следующей
// правке или «Back».
inline void flush() {
  if (draftDirty()) {
    if (DarktecPrefs::save(draftVal())) {
      draftDirty() = false;
      draftValid() = false;   // рендер снова читает флеш (уже новый)
    }
  }
  draftSaveAt() = 0;
}

// Вызывать из loop(): отложенная запись по 1.2 с тишины.
inline void loopTick() {
  const unsigned long t = draftSaveAt();
  if (t && (long)(millis() - t) >= 0) flush();
}

// ── Экран при зарядке (spec 0002) ───────────────────────────────────────────

// Фронт зарядки (не зарядка → зарядка). Пока экран был выключен, таймер
// авто-оффа никто не продлевал и он «в прошлом» — без сброса экран гаснет
// сразу после пробуждения. На true вызватель включает экран (turnOn),
// сбрасывает таймер и принудительную перерисовку; держится ли экран дальше
// во время зарядки — решает «Screen on chg» (гейт turnOff в DarktecDisplay).
inline bool chargeFront(bool& prev_charging, bool charging_now) {
  const bool front = charging_now && !prev_charging;
  prev_charging = charging_now;
  return front;
}

// ── Изменение значений (spec 0001 FR-8..11) ─────────────────────────────────

struct EditResult {
  bool changed;       // черновик обновлён и помечен к записи
  int  note_item;     // строка-заметка (0..5) или -1
  const char* note;   // текст заметки или NULL
};

// Шаг ±100 с переходом на противоположный край диапазона.
inline int cycleStep(int v, bool fwd, int vmin, int vmax) {
  v += fwd ? 100 : -100;
  if (v > vmax) v = vmin;
  if (v < vmin) v = vmax;
  return v;
}

// Правка значения строки i черновика (НЕ флеш). «2S: LTO only» — guard без
// изменения; заметки вызыватель показывает сам (note/note_item).
inline EditResult changeValue(int i, bool fwd) {
  EditResult r = { false, -1, NULL };
  if (i < 0 || i >= kRowBack) return r;
  if (!draftValid()) {
    draftVal() = DarktecPrefs::get();
    draftValid() = true;
  }
  DarktecPrefs::Settings& s = draftVal();
  if (i == 0) {
    s.capacity_mah = (uint16_t)cycleStep((int)s.capacity_mah, fwd,
        (int)DarktecPrefsFormat::kCapacityMin, (int)DarktecPrefsFormat::kCapacityMax);
  } else if (i == 1) {
    s.chem = (s.chem + (fwd ? 1 : 2)) % 3;
    if (s.chem != DarktecPrefsFormat::CHEM_LTO && s.cells > 1) {
      s.cells = 1;   // 2S доступна только для LTO — сброс с предупреждением (FR-10)
      r.note_item = 2; r.note = "cells: 1 (2S=LTO)";
    } else {
      r.note_item = 1; r.note = "after reboot";
    }
  } else if (i == 2) {
    if (s.cells == 2) {
      s.cells = 1;
      r.note_item = 2; r.note = "after reboot";
    } else if (s.chem == DarktecPrefsFormat::CHEM_LTO) {
      s.cells = 2;
      r.note_item = 2; r.note = "after reboot";
    } else {
      r.note_item = 2; r.note = "2S: LTO only";
      return r;   // значение не менялось — флеш не трогаем
    }
  } else if (i == 3) {
    s.module = (s.module + (fwd ? 1 : 3)) % 4;
    r.note_item = 3; r.note = "after reboot";
  } else if (i == 4) {
    s.charge_ma = (uint16_t)cycleStep((int)s.charge_ma, fwd,
        (int)DarktecPrefsFormat::kChargeMin, (int)DarktecPrefsFormat::kChargeMax);
  } else {  // i == 5
    // Screen on chg (spec 0002 FR-1): On = держать экран при зарядке.
    s.screen_sleep_chg = s.screen_sleep_chg ? 0 : 1;
  }
  r.changed = true;
  draftDirty() = true;
  draftSaveAt() = millis() + 1200;
  return r;
}

// ── Форматирование строк редактора ──────────────────────────────────────────

// Значение строки i («4000», «Li-ion», «2S», …); пустая строка у «Back».
// Читает shown() — черновик, пока есть несохранённые правки.
inline void formatRow(int i, char* buf, size_t len) {
  const DarktecPrefs::Settings& s = shown();
  buf[0] = '\0';
  static const char* chem_names[3] = { "Li-ion", "LiFePO4", "LTO" };
  static const char* mod_names[4] = { "HT-RA62", "E22-30S", "E22P-868", "E22-33S" };
  if (i == 0) {
    snprintf(buf, len, "%u", (unsigned)s.capacity_mah);
  } else if (i == 1) {
    snprintf(buf, len, "%s", chem_names[s.chem < 3 ? s.chem : 0]);
  } else if (i == 2) {
    snprintf(buf, len, "%uS", (unsigned)s.cells);
  } else if (i == 3) {
    snprintf(buf, len, "%s", mod_names[s.module < 4 ? s.module : 1]);
  } else if (i == 4) {
    snprintf(buf, len, "%u", (unsigned)s.charge_ma);
  } else if (i == 5) {
    snprintf(buf, len, "%s", s.screen_sleep_chg ? "Off" : "On");
  }
}

}  // namespace DarktecPrefsUI

#endif  // DARKTEC_PREFS_UI_H
