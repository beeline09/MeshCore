#ifdef DISPLAY_CLASS

#include "DarktecDisplay.h"
#include "DarktecBoard.h"

extern DarktecBoard board;

bool DarktecDisplay::statusDotWanted() const {
  // Только эфир LoRa: TX, приём пакета, короткий хвост после. Не BLE.
  return board.isLoRaActivity();
}

// ui-new FIRST: "< Connected >" на OLED — y = content_y+24 = 46, глиф 8 px.
static const int kDotX = 3;
static const int kDotY = 46 + (8 - 3) / 2;  // 48, вертикальный центр строки
static const int kDotS = 3;

void DarktecDisplay::paintStatusDot(bool on) {
  setColor(on ? LIGHT : DARK);
  fillRect(kDotX, kDotY, kDotS, kDotS);
}

void DarktecDisplay::startFrame(Color bkg) {
  _in_frame = true;
  _main_ui = false;
  SSD1306Display::startFrame(bkg);
}

void DarktecDisplay::drawRect(int x, int y, int w, int h) {
  // ui-new renderBatteryIndicator: 24×10 в (width-24-5, 0) — только home с шапкой.
  if (y == 0 && w == 24 && h == 10 && x == width() - 24 - 5) {
    _main_ui = true;
  }
  SSD1306Display::drawRect(x, y, w, h);
}

bool DarktecDisplay::isOn() {
  bool on = SSD1306Display::isOn();
  bool charging = board.isCharging();
  // KEEP_DISPLAY_ON_USB не будит погашенный OLED — только не даёт ему заснуть.
  if (charging && !_was_charging && !on) {
    turnOn();
    on = true;
  }
  _was_charging = charging;

  // Между кадрами UI (home ~5 с) доталкиваем только 3×3, буфер Adafruit живой.
  if (on && !_in_frame && _main_ui) {
    bool want = statusDotWanted();
    if (want != _dot_shown) {
      _dot_shown = want;
      paintStatusDot(want);
      SSD1306Display::endFrame();
    }
  }
  return on;
}

void DarktecDisplay::endFrame() {
  if (_main_ui) {
    if (board.isCharging()) {
      // Совпадает с ui-new renderBatteryIndicator: 24×10 в (width-29, 0).
      const int iconW = 24;
      const int iconH = 10;
      const int iconX = width() - iconW - 5;
      const int iconY = 0;

      setColor(DARK);
      fillRect(iconX + 8, iconY + 1, 8, 8);

      setColor(LIGHT);
      const int ox = iconX + 9;
      const int oy = iconY + 1;
      fillRect(ox + 3, oy + 0, 2, 2);
      fillRect(ox + 2, oy + 2, 2, 1);
      fillRect(ox + 0, oy + 3, 6, 2);
      fillRect(ox + 2, oy + 5, 2, 1);
      fillRect(ox + 1, oy + 6, 2, 2);

      setColor(LIGHT);
      drawRect(iconX, iconY, iconW, iconH);
      fillRect(iconX + iconW, iconY + (iconH / 4), 3, iconH / 2);
    }

    _dot_shown = statusDotWanted();
    if (_dot_shown) {
      paintStatusDot(true);
    }
  } else {
    _dot_shown = false;
  }

  SSD1306Display::endFrame();
  _in_frame = false;
}

#endif
