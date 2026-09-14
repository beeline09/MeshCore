#ifdef DISPLAY_CLASS

#include "DarktecDisplay.h"
#include "DarktecBoard.h"

extern DarktecBoard board;

bool DarktecDisplay::statusDotWanted() const {
  // LoRa airtime only (TX / RX / short tail). Not BLE.
  return board.isLoRaActivity();
}

// ui-new FIRST: "< Connected >" on OLED — y = content_y+24 = 46, glyph 8 px.
static const int kDotX = 3;
static const int kDotY = 46 + (8 - 3) / 2;  // 48, vertical centre of that row
static const int kDotS = 3;

void DarktecDisplay::paintStatusDot(bool on) {
  setColor(on ? UIColor::title_txt : UIColor::window_bkg);
  fillRect(kDotX, kDotY, kDotS, kDotS);
}

void DarktecDisplay::startFrame(ColorVal bkg) {
  _in_frame = true;
  _main_ui = false;
  SSD1306Display::startFrame(bkg);
}

void DarktecDisplay::drawRect(int x, int y, int w, int h) {
  // ui-new renderBatteryIndicator: 24×10 at (width-24-5, 0) — home header only.
  if (y == 0 && w == 24 && h == 10 && x == width() - 24 - 5) {
    _main_ui = true;
  }
  SSD1306Display::drawRect(x, y, w, h);
}

bool DarktecDisplay::isOn() {
  bool on = SSD1306Display::isOn();
  bool charging = board.isCharging();
  // KEEP_DISPLAY_ON_USB does not wake a dark OLED — it only prevents sleep.
  if (charging && !_was_charging && !on) {
    turnOn();
    on = true;
  }
  _was_charging = charging;

  // Between UI frames (home ~5 s) push only the 3×3; Adafruit buffer stays live.
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
