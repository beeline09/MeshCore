// Darktec overlay of examples/simple_repeater/UITask.cpp (spec 0002, FR-15..21).
// The original examples/ file is unchanged: this file is swapped in by the
// build filter of the Darktec_repeater* envs. Same UITask class/API as the
// original header — pages and interaction state live in file-statics.
//
// Pages (carousel, click = next, double = prev):
//   RADIO — radio params + node name (long press = power off, as before);
//   NEIGH — neighbours table sorted by SNR (long = discover, triple = clear
//           + discover again);
//   NOISE — noise/SNR graph (long = NF<->SNR, triple = speed);
//   INA   — INA3221 table (SP/DC/Bat) + ETA;
//   SETTINGS — "[ Settings ]" row, long press opens the folder:
//           [ Advert ] sends a self-advertisement, [ Darktec ] opens the
//           Darktec settings editor (same as the client), [ Back ] returns.

#include "UITask.h"
#include "MyMesh.h"
#include "target.h"
#include <Arduino.h>
#include <helpers/CommonCLI.h>
#include <helpers/ui/UIScreen.h>   // KEY_* codes
#include <string.h>

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      20000  // 20 seconds
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds
#define POWEROFF_DELAY 3000

#ifdef DARKTEC
#define REP_PAGES 5
#else
#define REP_PAGES 1
#endif

// 'meshcore', 128x13px
static const uint8_t meshcore_logo [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe,
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe,
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc,
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00,
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00,
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8,
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8,
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0,
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00,
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00,
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8,
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8,
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8,
};

#ifdef DARKTEC
#include "DarktecPrefs.h"
#include "DarktecPowerState.h"

extern MyMesh the_mesh;

// ---- карусель и состояние Darktec-настроек ----
enum RepPage { P_RADIO = 0, P_NEIGH, P_NOISE, P_INA, P_SETTINGS };
static int      rep_page = P_RADIO;

// настройки: 0 = корень страницы, 1 = папка (Advert/Darktec/Back), 2 = Darktec
static int      set_state = 0;
static int      set_sel = 0;      // курсор папки
static int      d_sel = 0;        // курсор редактора Darktec (7 строк)
static bool     d_editing = false;
static unsigned long d_note_until = 0;
static int      d_note_item = 0;
static char     d_note[24] = "";
static char     rep_toast[24] = "";
static unsigned long rep_toast_until = 0;

// ---- графики шума/SNR (spec 0002 FR-6..10) ----
static constexpr int NOISE_SAMPLES = 64;
static int16_t       nf_hist[NOISE_SAMPLES] = {};
static int16_t       snr_hist[NOISE_SAMPLES] = {};
static uint8_t       noise_head = 0, noise_count = 0;
static unsigned long next_noise_sample = 0;
static bool          noise_show_snr = false;

// ---- EMA тока батареи для ETA (FR-14) ----
static float    bat_i_ema = 0;
static bool     bat_i_ema_ready = false;

static unsigned long noiseSpeedMs() {
  static const unsigned long speeds[4] = { 100, 250, 500, 1000 };
  uint8_t idx = DarktecPrefs::get().noise_speed;
  if (idx > 3) idx = DarktecPrefsFormat::kNoiseSpeedDefault;
  return speeds[idx];
}

static void darktecNote(int item, const char* text) {
  d_note_item = item;
  snprintf(d_note, sizeof(d_note), "%s", text ? text : "");
  d_note_until = millis() + 4000;
}

static void repToast(const char* text) {
  snprintf(rep_toast, sizeof(rep_toast), "%s", text ? text : "");
  rep_toast_until = millis() + 800;
}

// Обрезка хвостовых нулей: 0.400 -> 0.4, 0.000 -> 0.0.
static void fmtTrimmed(char* buf, size_t len, float v, int decimals) {
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%.*f", decimals, (double)v);
  char* dot = strchr(tmp, '.');
  if (dot) {
    char* end = tmp + strlen(tmp) - 1;
    while (end > dot + 1 && *end == '0') *end-- = '\0';
  }
  snprintf(buf, len, "%s", tmp);
}

// Инверсная метка (белый блок + чёрный текст, скругление — срез 1 px углов).
static void drawInvLabel(DisplayDriver& display, const char* text, int x, int y, bool selected) {
  const int w = display.getTextWidth(text) + 2;
  if (selected) {
    display.setColor(UIColor::primary_txt);
    display.fillRect(x, y, w, 10);
    display.setColor(UIColor::window_bkg);
    display.fillRect(x, y, 1, 1);
    display.fillRect(x + w - 1, y, 1, 1);
    display.fillRect(x, y + 9, 1, 1);
    display.fillRect(x + w - 1, y + 9, 1, 1);
    display.setCursor(x + 1, y + 1);
    display.print(text);
  } else {
    display.setColor(UIColor::secondary_txt);
    display.setCursor(x + 1, y + 1);
    display.print(text);
  }
}

// График шума/SNR (FR-6..10): трасса 64×2 px между y0 и foot_y.
static void drawNoiseGraph(DisplayDriver& display, int plot_y, int foot_y) {
  const unsigned long speed = noiseSpeedMs();
  if ((long)(millis() - next_noise_sample) >= 0) {
    nf_hist[noise_head] = (int16_t)radio_driver.getNoiseFloor();
    snr_hist[noise_head] = (int16_t)(radio_driver.getLastSNR() * 10.0f);
    noise_head = (noise_head + 1) % NOISE_SAMPLES;
    if (noise_count < NOISE_SAMPLES) noise_count++;
    next_noise_sample = millis() + speed;
  }

  const int16_t* hist = noise_show_snr ? snr_hist : nf_hist;
  int lo = 0, hi = 0;
  for (int i = 0; i < noise_count; i++) {
    int sum = 0, n = 0;
    for (int k = i - 1; k <= i + 1; k++) {
      if (k < 0 || k >= noise_count) continue;
      sum += hist[(noise_head + NOISE_SAMPLES - noise_count + k) % NOISE_SAMPLES];
      n++;
    }
    int v = n ? sum / n : 0;
    if (i == 0)      { lo = v; hi = v; }
    else if (v < lo) { lo = v; }
    else if (v > hi) { hi = v; }
  }
  if (hi - lo < 8) {
    int mid = (lo + hi) / 2;
    lo = mid - 4;
    hi = mid + 4;
  }

  const int plot_h = foot_y - 2 - plot_y;
  const int bar_w = display.width() / NOISE_SAMPLES;
  const int y_bot = plot_y + plot_h - 2;

  display.setColor(UIColor::secondary_txt);
  display.fillRect(0, plot_y + plot_h - 1, display.width(), 1);
  display.setColor(UIColor::primary_txt);
  int prev_y = -1;
  for (int i = 0; i < noise_count; i++) {
    int sum = 0, n = 0;
    for (int k = i - 1; k <= i + 1; k++) {
      if (k < 0 || k >= noise_count) continue;
      sum += hist[(noise_head + NOISE_SAMPLES - noise_count + k) % NOISE_SAMPLES];
      n++;
    }
    int v = n ? sum / n : 0;
    int y = y_bot - (v - lo) * (y_bot - plot_y) / (hi - lo);
    if (y < plot_y) y = plot_y;
    if (y > y_bot) y = y_bot;
    if (prev_y < 0) {
      display.fillRect(i * bar_w, y, bar_w, 2);
    } else {
      int top = (y < prev_y) ? y : prev_y;
      int bot = (y < prev_y) ? prev_y : y;
      display.fillRect(i * bar_w, top, bar_w, bot - top + 2);
    }
    prev_y = y;
  }
}

// Строка значений под графиком: NF слева, SNR справа, выбранное — инверсией.
static void drawNoiseFooter(DisplayDriver& display) {
  char lbuf[16], rbuf[16];
  snprintf(lbuf, sizeof(lbuf), "NF:%ddBm", (int)radio_driver.getNoiseFloor());
  snprintf(rbuf, sizeof(rbuf), "SNR:%.1fdB", (double)radio_driver.getLastSNR());
  drawInvLabel(display, lbuf, 0, display.height() - 10, !noise_show_snr);
  drawInvLabel(display, rbuf, display.width() - display.getTextWidth(rbuf) - 2,
               display.height() - 10, noise_show_snr);
}

// Таблица INA3221 + ETA (FR-11..14). rows: 2 (клиент, DC/Bat) или 3
// (репитер, SP/DC/Bat). Возвращает y, где закончила.
static int drawInaTable(DisplayDriver& display, int rows) {
  float mv[3] = { 0, 0, 0 }, ma[3] = { 0, 0, 0 };
  bool ok[3] = { false, false, false };
  for (int ch = 0; ch < 3; ch++) {
    ok[ch] = board.readInaChannel(ch, &mv[ch], &ma[ch]);
  }

  static const char* names[3] = { "SP", "DC", "Bat" };
  const int first = (rows == 3) ? 0 : 1;
  const int ty = 14;                       // точки страницы скрыты
  const int th = 9 + rows * 10 + 2;

  display.setColor(UIColor::primary_txt);
  display.drawRect(0, ty, display.width() - 1, th);

  // Шапка: инвертированная полоса, чёрные линии сетки шапки.
  display.fillRect(1, ty + 1, display.width() - 3, 8);
  display.setColor(UIColor::window_bkg);
  static const int hlines[3] = { 21, 49, 88 };
  for (int i = 0; i < 3; i++) display.fillRect(hlines[i], ty + 1, 1, 8);
  display.fillRect(1, ty + 9, display.width() - 3, 1);
  display.drawTextCentered(10, ty + 1, "CH");
  display.drawTextCentered(35, ty + 1, "V");
  display.drawTextCentered(68, ty + 1, "A");
  display.drawTextCentered(107, ty + 1, "W");

  display.setColor(UIColor::primary_txt);
  // разделители строк каналов
  for (int r = 1; r < rows; r++) {
    display.fillRect(1, ty + 9 + r * 10, display.width() - 3, 1);
  }
  for (int i = 0; i < 3; i++) {
    display.fillRect(hlines[i], ty + 10, 1, th - 11);
  }

  char buf[12];
  for (int r = 0; r < rows; r++) {
    const int y = ty + 10 + r * 10 + 1;
    const int ch = first + r;
    display.setCursor(3, y);
    display.print(ok[ch] ? names[ch] : "--");
    if (ok[ch]) {
      fmtTrimmed(buf, sizeof(buf), mv[ch], 2);
      display.drawTextRightAlign(47, y, buf);
      fmtTrimmed(buf, sizeof(buf), ma[ch] / 1000.0f, 3);
      display.drawTextRightAlign(85, y, buf);
      fmtTrimmed(buf, sizeof(buf), mv[ch] * (ma[ch] / 1000.0f), 3);
      display.drawTextRightAlign(125, y, buf);
    }
  }
  return ty + th;
}

// Строка ETA: треугольник направления, время, SOC/температура МК.
static void drawEtaLine(DisplayDriver& display, int ey) {
  const uint16_t batt_mv = board.readBattMvRaw();
  const uint8_t soc = DarktecPowerState::socPercent(batt_mv);
  const bool charging = board.isCharging();
  const DarktecPrefs::Settings& prefs = DarktecPrefs::get();

  int mins = -1;
  bool resting_ext = false;
  if (board.isChargeFull()) {
    mins = -2;
  } else if (charging) {
    const float remain = (100.0f - soc) / 100.0f * prefs.capacity_mah;
    if (prefs.charge_ma > 0) mins = (int)(remain / prefs.charge_ma * 60.0f + 0.5f);
  } else if (board.isExternalPowered()) {
    resting_ext = true;
  } else {
    float ii = bat_i_ema;
    if (ii < 0) ii = -ii;
    if (ii >= 10.0f) mins = (int)((soc / 100.0f * prefs.capacity_mah) / ii * 60.0f + 0.5f);
  }

  const int tx0 = 3, ty0 = ey + 1;
  display.setColor(UIColor::primary_txt);
  if (charging) {
    display.fillRect(tx0 + 2, ty0, 1, 1);
    display.fillRect(tx0 + 1, ty0 + 1, 3, 1);
    display.fillRect(tx0, ty0 + 2, 5, 2);
  } else if (resting_ext || mins == -2) {
    display.fillRect(tx0 + 1, ty0 + 1, 3, 3);
  } else {
    display.fillRect(tx0, ty0, 5, 2);
    display.fillRect(tx0 + 1, ty0 + 2, 3, 1);
    display.fillRect(tx0 + 2, ty0 + 3, 1, 1);
  }

  char ebuf[20];
  if (mins == -2) {
    snprintf(ebuf, sizeof(ebuf), "E full");
  } else if (resting_ext) {
    snprintf(ebuf, sizeof(ebuf), "E ext");
  } else if (mins < 0) {
    snprintf(ebuf, sizeof(ebuf), "E --");
    } else {
    // Полная лестница времени (spec 0002, замечание владельца): минуты →
    // часы (до 10 ч с минутами) → часы → сутки → месяцы → годы. Пороги: сутки/30 дней/12 месяцев. При
    // слабом токе разряда оценка честно уходит в дни и месяцы.
    const long h = mins / 60;
    if (mins < 600)                 snprintf(ebuf, sizeof(ebuf), "E %ldh%02ldm", h, (long)(mins % 60));
    else if (h < 24)                snprintf(ebuf, sizeof(ebuf), "E %ldh", h);
    else if (h < 24L * 30)         snprintf(ebuf, sizeof(ebuf), "E %ldd", h / 24);
    else if (h < 24L * 365)     snprintf(ebuf, sizeof(ebuf), "E %ldmo", h / 30);
    else                            snprintf(ebuf, sizeof(ebuf), "E %ldy", h / 365);
  }
  display.setCursor(tx0 + 9, ey);
  display.print(ebuf);
  display.setColor(UIColor::secondary_txt);
  int mcu_c = (int)(board.getMCUTemperature() + 0.5f);
  snprintf(ebuf, sizeof(ebuf), "SOC %d%%-%dC", (int)soc, mcu_c);
  display.drawTextRightAlign(display.width() - 2, ey, ebuf);
}

// Редактор Darktec (те же 7 строк, что у клиента): смена значения i, fwd.
static void darktecChangeValue(int i, bool fwd) {
  DarktecPrefs::Settings s = DarktecPrefs::get();
  if (i == 0) {
    int cap = (int)s.capacity_mah + (fwd ? 100 : -100);
    if (cap > (int)DarktecPrefsFormat::kCapacityMax) cap = (int)DarktecPrefsFormat::kCapacityMin;
    if (cap < (int)DarktecPrefsFormat::kCapacityMin) cap = (int)DarktecPrefsFormat::kCapacityMax;
    s.capacity_mah = (uint16_t)cap;
  } else if (i == 1) {
    s.chem = (s.chem + (fwd ? 1 : 2)) % 3;
    if (s.chem != DarktecPrefsFormat::CHEM_LTO && s.cells > 1) {
      s.cells = 1;
      darktecNote(2, "cells: 1 (2S=LTO)");
    } else {
      darktecNote(1, "after reboot");
    }
  } else if (i == 2) {
    if (s.cells == 2) {
      s.cells = 1;
      darktecNote(2, "after reboot");
    } else if (s.chem == DarktecPrefsFormat::CHEM_LTO) {
      s.cells = 2;
      darktecNote(2, "after reboot");
    } else {
      darktecNote(2, "2S: LTO only");
      return;
    }
  } else if (i == 3) {
    s.module = (s.module + (fwd ? 1 : 3)) % 4;
    darktecNote(3, "after reboot");
  } else if (i == 4) {
    int ma = (int)s.charge_ma + (fwd ? 100 : -100);
    if (ma > (int)DarktecPrefsFormat::kChargeMax) ma = (int)DarktecPrefsFormat::kChargeMin;
    if (ma < (int)DarktecPrefsFormat::kChargeMin) ma = (int)DarktecPrefsFormat::kChargeMax;
    s.charge_ma = (uint16_t)ma;
  } else if (i == 5) {
    s.screen_sleep_chg = s.screen_sleep_chg ? 0 : 1;
  } else {
    return;
  }
  DarktecPrefs::save(s);
}

static constexpr int D_ROWS = 7;  // 6 параметров + Back
static constexpr int F_ROWS = 3;  // [ Advert ] / [ Darktec ] / [ Back ]

// Отрисовка страницы настроек (set_state 0/1/2). Возвращает интервал.
static int renderSettingsPage(DisplayDriver& display) {
  display.setTextSize(1);
  display.setColor(UIColor::primary_txt);
  display.setCursor(0, 0);
  display.print(the_mesh.getNodeName());

  if (set_state == 0) {
    display.setCursor(0, 20);
    display.print("[ Settings ]");
    display.setColor(UIColor::secondary_txt);
    display.drawTextCentered(display.width() / 2, display.height() - 11, "open: long press");
    return 1000;
  }

  int y = 20;
  if (set_state == 1) {
    static const char* rows[F_ROWS] = { "[ Advert ]", "[ Darktec ]", "[ Back ]" };
    for (int i = 0; i < F_ROWS; i++, y += 11) {
      display.setColor(UIColor::primary_txt);
      display.setCursor(0, y);
      display.print(set_sel == i ? ">" : " ");
      display.setCursor(6, y);
      display.print(rows[i]);
    }
    return 1000;
  }

  // set_state == 2: редактор Darktec (как у клиента).
  const DarktecPrefs::Settings& prefs = DarktecPrefs::get();
  static const char* chem_names[3] = { "Li-ion", "LiFePO4", "LTO" };
  static const char* mod_names[4] = { "HT-RA62", "E22-30S", "E22P-868", "E22-33S" };
  static const char* labels[D_ROWS] = { "Capacity, mAh", "Batt type", "Cells", "Radio mod",
                                        "Charge, mA", "Screen on chg", "Back" };
  y = 16;
  for (int i = 0; i < D_ROWS; i++, y += 7) {
    const char* label = labels[i];
    if (i > 0 && i < 6 && i == d_note_item + 1 && d_note[0] && millis() < d_note_until) {
      label = d_note;
    }
    display.setColor(UIColor::primary_txt);
    display.setCursor(0, y);
    display.print(d_sel == i ? ">" : " ");
    display.setCursor(6, y);
    display.print(label);
    char raw[12] = "";
    if (i == 0) {
      snprintf(raw, sizeof(raw), "%u", (unsigned)prefs.capacity_mah);
    } else if (i == 1) {
      snprintf(raw, sizeof(raw), "%s", chem_names[prefs.chem < 3 ? prefs.chem : 0]);
    } else if (i == 2) {
      snprintf(raw, sizeof(raw), "%uS", (unsigned)prefs.cells);
    } else if (i == 3) {
      snprintf(raw, sizeof(raw), "%s", mod_names[prefs.module < 4 ? prefs.module : 1]);
    } else if (i == 4) {
      snprintf(raw, sizeof(raw), "%u", (unsigned)prefs.charge_ma);
    } else if (i == 5) {
      snprintf(raw, sizeof(raw), "%s", prefs.screen_sleep_chg ? "Off" : "On");
    }
    if (raw[0]) {
      char shown[16];
      if (d_editing && d_sel == i) {
        snprintf(shown, sizeof(shown), "[%s]", raw);
        display.drawTextRightAlign(display.width() - 2, y, shown);
      } else {
        display.drawTextRightAlign(display.width() - 2, y, raw);
      }
    }
  }
  return d_editing ? 300 : 1000;
}
#endif  // DARKTEC

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _prevBtnState = HIGH;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _started_at = millis();
  _node_prefs = node_prefs;
  _display->turnOn();

#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  user_btn.begin();
#endif

  // strip off dash and commit hash by changing dash to null terminator
  // e.g: v1.2.3-abcdef -> v1.2.3
  char *version = strdup(firmware_version);
  char *dash = strchr(version, '-');
  if(dash){
    *dash = 0;
  }

  // v1.2.3 (1 Jan 2025)
  snprintf(_version_info, sizeof(_version_info), "%s (%s)", version, build_date);
  free(version);
}

static unsigned long rep_refresh_ms = 1000;  // интервал следующей перерисовки

void UITask::renderCurrScreen() {
  char tmp[80];
  if (millis() < _started_at + BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 22, website);

    // version info
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 35, _version_info);

    // node type
    const char* node_type = "< Repeater >";
    _display->drawTextCentered(_display->width() / 2, 48, node_type);
    return;
  } else if (_powering_off_at > 0) {
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // Powering off
    const char* poweroff_string = "Turning OFF";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width()/2, 48, poweroff_string);
    return;
  }

#ifdef DARKTEC
  rep_refresh_ms = 1000;
  _display->setTextSize(1);
  _display->setColor(UIColor::primary_txt);
  _display->setCursor(0, 0);
  _display->print(_node_prefs->node_name);

  // точки карусели (на INA-странице скрыты — таблица+ETA занимают всё место)
  if (rep_page != P_INA) {
    int dot_y = 10;
    int x = _display->width() / 2 - 5 * (REP_PAGES - 1);
    for (int i = 0; i < REP_PAGES; i++) {
      if (i == rep_page) {
        _display->fillRect(x - 1, dot_y - 1, 3, 3);
      } else {
        _display->fillRect(x, dot_y, 1, 1);
      }
      x += 10;
    }
  }

  switch (rep_page) {
    case P_NEIGH: {
      NeighbourInfo nb[5];
      int n = the_mesh.getNeighboursSorted(nb, 5);
      _display->setColor(UIColor::primary_txt);
      snprintf(tmp, sizeof(tmp), "Neighbours: %d", n);
      _display->setCursor(0, 16);
      _display->print(tmp);
      int y = 28;
      for (int i = 0; i < n; i++, y += 9) {
        char name[12];
        mesh::Utils::toHex(name, nb[i].id.pub_key, 4);
        char row[24];
        snprintf(row, sizeof(row), "%s  %.1f", name, nb[i].snr / 4.0f);
        _display->setCursor(0, y);
        _display->print(row);
      }
      if (n == 0) {
        _display->setColor(UIColor::secondary_txt);
        _display->drawTextCentered(_display->width() / 2, 40, "hold: find");
      }
      break;
    }
    case P_NOISE: {
      drawNoiseGraph(*_display, 20, _display->height() - 9);
      drawNoiseFooter(*_display);
      rep_refresh_ms = noiseSpeedMs();   // перерисовка синхронна сэмплингу
      break;
    }
    case P_INA: {
      const int ey = drawInaTable(*_display, 3);
      drawEtaLine(*_display, ey + 1);   // отступ 1 px между таблицей и ETA
      break;
    }
    case P_SETTINGS: {
      rep_refresh_ms = renderSettingsPage(*_display);
      break;
    }
    default: {
      // P_RADIO — как исходный экран: имя уже выведено, частота/модуляция.
      _display->setColor(UIColor::primary_txt);
      _display->setCursor(0, 20);
      sprintf(tmp, "FREQ: %06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
      _display->print(tmp);
      _display->setCursor(0, 30);
      sprintf(tmp, "BW: %03.2f CR: %d", _node_prefs->bw, _node_prefs->cr);
      _display->print(tmp);
      break;
    }
  }

  if (rep_toast[0] && millis() < rep_toast_until) {
    _display->setColor(UIColor::primary_txt);
    const int w = _display->getTextWidth(rep_toast) + 6;
    const int x = (_display->width() - w) / 2;
    _display->fillRect(x, 26, w, 12);
    _display->setColor(UIColor::window_bkg);
    _display->drawTextCentered(_display->width() / 2, 28, rep_toast);
  }
#endif
}

// Обработка клавиш карусели/меню (spec 0002). Возвращает true, если клавиша
// поглощена страницей (не переключает карусель).
#ifdef DARKTEC
static bool repHandleKey(UITask* task, char c, DisplayDriver* display) {
  if (c == KEY_ENTER) {
    if (set_state == 2) {
      if (d_sel == 6) {
        set_state = 1;  // Back — из редактора Darktec в папку настроек
        d_sel = 1;
        d_editing = false;
      } else {
        d_editing = !d_editing;  // правка значения ([скобки])
      }
      return true;
    }
    if (set_state == 1) {
      if (set_sel == 0) {
        the_mesh.sendSelfAdvertisement(0, true);
        repToast("Advert sent");
      } else if (set_sel == 1) {
        set_state = 2;  // [ Darktec ] — открыть редактор
        d_sel = 0;
        d_editing = false;
      } else {
        set_state = 0;  // [ Back ] — на корень страницы настроек
        set_sel = 0;
      }
      return true;
    }
    if (rep_page == P_SETTINGS) {
      set_state = 1;  // долгое на «[ Settings ]» — открыть папку
      set_sel = 0;
      return true;
    }
    if (rep_page == P_NOISE) {
      noise_show_snr = !noise_show_snr;
      return true;
    }
    if (rep_page == P_NEIGH) {
      the_mesh.sendNodeDiscoverReq();
      repToast("Searching...");
      return true;
    }
    if (rep_page == P_RADIO) {
      return false;  // долгое на «Радио» — выключение (как раньше)
    }
    return true;
  }
  if (c == KEY_SELECT) {
    if (set_state == 2 && d_editing) {
      darktecChangeValue(d_sel, false);
      return true;
    }
    if (rep_page == P_NOISE) {
      DarktecPrefs::Settings s = DarktecPrefs::get();
      s.noise_speed = (s.noise_speed + 1) % (DarktecPrefsFormat::kNoiseSpeedMax + 1);
      DarktecPrefs::save(s);
      next_noise_sample = 0;
      static const char* speed_labels[4] = { "100ms", "250ms", "500ms", "1000ms" };
      char msg[20];
      snprintf(msg, sizeof(msg), "Noise: %s", speed_labels[s.noise_speed]);
      repToast(msg);
      return true;
    }
    if (rep_page == P_NEIGH) {
      // тройное — очистить список и запросить заново (FR-17)
      the_mesh.clearNeighbours();
      the_mesh.sendNodeDiscoverReq();
      repToast("Cleared, search");
      return true;
    }
    return true;
  }
  // NEXT/PREV: внутри меню — курсор; снаружи — листание карусели.
  int rows = 0;
  if (set_state == 2) rows = D_ROWS;
  else if (set_state == 1) rows = F_ROWS;
  if (rows > 0) {
    if (c == KEY_NEXT) {
      if (set_state == 2) d_sel = (d_sel + 1) % rows;
      else set_sel = (set_sel + 1) % rows;
      return true;
    }
    if (c == KEY_PREV) {
      if (set_state == 2) d_sel = (d_sel + rows - 1) % rows;
      else set_sel = (set_sel + rows - 1) % rows;
      return true;
    }
    return true;  // меню глотает остальные клавиши
  }
  if (c == KEY_NEXT) {
    rep_page = (rep_page + 1) % REP_PAGES;
    return true;
  }
  if (c == KEY_PREV) {
    rep_page = (rep_page + REP_PAGES - 1) % REP_PAGES;
    return true;
  }
  return true;
}
#endif

void UITask::loop() {
#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  int ev = user_btn.check();
  char key = 0;
  if (ev == BUTTON_EVENT_CLICK) {
    key = KEY_NEXT;
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    key = KEY_ENTER;
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    key = KEY_PREV;
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    key = KEY_SELECT;
  }
  if (key != 0) {
    if (!_display->isOn()) {
      _display->turnOn();   // любое нажатие будит экран
      _auto_off = millis() + AUTO_OFF_MILLIS;
      key = 0;              // первое нажатие только будит
    } else {
      _auto_off = millis() + AUTO_OFF_MILLIS;
#ifdef DARKTEC
      // Долгое на «Радио» — выключение (как в исходном UI); остальные
      // страницы обрабатывают клавиши сами.
      const bool poweroff = (key == KEY_ENTER && rep_page == P_RADIO && set_state == 0);
      if (!poweroff) {
        repHandleKey(this, key, _display);
        key = 0;
      }
#endif
    }
  }
  if (key != 0) {
    // ключевая последовательность для выключения (только RADIO + долгое)
#ifdef DARKTEC
    if (key == KEY_ENTER) {
      _display->turnOn();
      Serial.println("Powering Off");
      _powering_off_at = millis() + POWEROFF_DELAY;
      key = 0;
    }
#else
    if (key == KEY_ENTER) {
      _display->turnOn();
      Serial.println("Powering Off");
      _powering_off_at = millis() + POWEROFF_DELAY;
      key = 0;
    }
#endif
  }
#endif

  if (_display->isOn()) {
    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + rep_refresh_ms;
    }
#ifdef DARKTEC
    // spec 0002: на странице шума и в открытых меню панель не гаснем.
    const bool hold = (rep_page == P_NOISE) || set_state > 0 || d_editing;
    if (millis() > _auto_off && !hold) {
      _display->turnOff();
    }
#else
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

  if (_powering_off_at > 0) { // power off timer armed
#if defined(LED_PIN) && defined(LED_STATE_ON)
    digitalWrite(LED_PIN, LED_STATE_ON); // switch on the led until poweroff
    delay(1000);
    digitalWrite(LED_PIN, !LED_STATE_ON); // off LED
#endif
    if (millis() > _powering_off_at) {
      _board->powerOff();  // should not return
    }
  }
}
