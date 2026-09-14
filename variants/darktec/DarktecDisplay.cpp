#ifdef DISPLAY_CLASS

#include "DarktecDisplay.h"
#include "DarktecBoard.h"
#include "glcdfont6x8.h"

extern DarktecBoard board;

ColorVal UIColor::window_bkg = SSD1306_BLACK;
ColorVal UIColor::title_bkg = SSD1306_BLACK;
ColorVal UIColor::title_txt = SSD1306_WHITE;
ColorVal UIColor::primary_txt = SSD1306_WHITE;
ColorVal UIColor::secondary_txt = SSD1306_WHITE;
ColorVal UIColor::warning_txt = SSD1306_WHITE;
ColorVal UIColor::popup_bkg = SSD1306_BLACK;
ColorVal UIColor::popup_txt = SSD1306_WHITE;
ColorVal UIColor::corp_blue = SSD1306_WHITE;

bool DarktecDisplay::i2c_probe(TwoWire& wire, uint8_t addr) {
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

bool DarktecDisplay::begin() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;
  }
#ifdef DISPLAY_ROTATION
  display.setRotation(DISPLAY_ROTATION);
#endif
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS, true, false)
            && i2c_probe(Wire, DISPLAY_ADDRESS);
  if (ok) {
    display.setFont(&glcdfont6x8);
    display.setTextWrap(false);
  }
  return ok;
}

void DarktecDisplay::turnOn() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;
    if (_peripher_power) begin();
  }
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void DarktecDisplay::turnOff() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  if (_isOn) {
    if (_peripher_power) {
#if PIN_OLED_RESET >= 0
      digitalWrite(PIN_OLED_RESET, LOW);
#endif
      _peripher_power->release();
    }
    _isOn = false;
  }
}

void DarktecDisplay::clear() {
  display.clearDisplay();
  display.display();
}

bool DarktecDisplay::statusDotWanted() const {
  return board.isLoRaActivity();
}

static const int kDotX = 3;
static const int kDotS = 3;
static const int kDotY = (64 - kDotS) / 2;  // 30, середина 64px OLED; X не трогаем

void DarktecDisplay::paintStatusDot(bool on) {
  setColor(on ? UIColor::title_txt : UIColor::window_bkg);
  fillRect(kDotX, kDotY, kDotS, kDotS);
}

void DarktecDisplay::flushFrame() {
  display.display();
}

void DarktecDisplay::startFrame(ColorVal bkg) {
  (void)bkg;
  _in_frame = true;
  _main_ui = false;
  display.clearDisplay();
  _color = SSD1306_WHITE;
  display.setFont(&glcdfont6x8);
  display.setTextWrap(false);
  display.setTextColor(_color);
  _font_size = 1;
  display.setTextSize(1);
}

void DarktecDisplay::setTextSize(int sz) {
  _font_size = (uint8_t)sz;
  display.setTextSize(sz);
}

void DarktecDisplay::setColor(ColorVal c) {
  _color = c;
  display.setTextColor(_color);
}

void DarktecDisplay::setCursor(int x, int y) {
  _cursor_y_raw = y;
  // GFXfont cursor is baseline; UI callers use top-left.
  display.setCursor(x, y + (_font_size * 7));
}

void DarktecDisplay::print(const char* str) {
  display.print(str);
}

void DarktecDisplay::printWordWrap(const char* str, int max_width) {
  display.setTextWrap(false);
  const int char_w = 6 * _font_size;
  const int line_h = 8 * _font_size;
  int max_chars = char_w > 0 ? max_width / char_w : 0;
  if (max_chars <= 0) {
    print(str);
    return;
  }
  if (max_chars * char_w > max_width) max_chars--;

  int len = (int)strlen(str);
  int pos = 0;
  char line_buf[48];

  while (pos < len && _cursor_y_raw + line_h <= height()) {
    int remaining = len - pos;
    int take = remaining < max_chars ? remaining : max_chars;
    int break_at = pos + take;
    if (remaining > max_chars) {
      for (int i = pos + take; i > pos; i--) {
        if (str[i] == ' ') { break_at = i; break; }
      }
    }
    int seg_len = break_at - pos;
    if (seg_len < 1) {
      seg_len = take;
      break_at = pos + take;
    }
    if (seg_len > (int)sizeof(line_buf) - 1) seg_len = (int)sizeof(line_buf) - 1;
    memcpy(line_buf, str + pos, seg_len);
    line_buf[seg_len] = 0;
    print(line_buf);

    pos = break_at;
    if (pos < len && str[pos] == ' ') pos++;
    if (pos < len) {
      _cursor_y_raw += line_h;
      if (_cursor_y_raw + line_h <= height()) setCursor(0, _cursor_y_raw);
    }
  }
}

void DarktecDisplay::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void DarktecDisplay::drawRect(int x, int y, int w, int h) {
  if (y == 0 && w == 24 && h == 10 && x == width() - 24 - 5) {
    _main_ui = true;
  }
  display.drawRect(x, y, w, h, _color);
}

void DarktecDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t DarktecDisplay::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void DarktecDisplay::translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
  size_t j = 0;
  for (size_t i = 0; src[i] != 0 && j < dest_size - 1; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c >= 32 && c <= 126) {
      dest[j++] = (char)c;
      continue;
    }
    if (c == 0xC2 || c == 0xC3 || c == 0xD0 || c == 0xD1 || c == 0xD2) {
      unsigned char n = (unsigned char)src[i + 1];
      if (n == 0) break;  // truncated sequence — drop the lead byte
      i++;
      char cc = 0;
      switch (c) {
        case 0xC2: cc = (char)n; break;
        case 0xC3: cc = (char)(n | 0xC0); break;
        case 0xD0:
          if      (n == 129) cc = 168;
          else if (n == 132) cc = 170;
          else if (n == 134) cc = 178;
          else if (n == 135) cc = 175;
          else if (n > 143 && n < 192) cc = (char)(n + 48);
          break;
        case 0xD1:
          if      (n == 145) cc = 184;
          else if (n == 148) cc = 186;
          else if (n == 150) cc = 179;
          else if (n == 151) cc = 191;
          else if (n > 127 && n < 144) cc = (char)(n + 112);
          break;
        case 0xD2:
          if      (n == 144) cc = 165;
          else if (n == 145) cc = 180;
          break;
      }
      dest[j++] = (cc != 0) ? cc : '?';
      continue;
    }
    if (c >= 0x80) {
      dest[j++] = '?';
      while (src[i + 1] && (((unsigned char)src[i + 1]) & 0xC0) == 0x80)
        i++;
    }
  }
  dest[j] = 0;
}

bool DarktecDisplay::isOn() {
  bool on = _isOn;
  bool charging = board.isCharging();
  if (charging && !_was_charging && !on) {
    turnOn();
    on = true;
  }
  _was_charging = charging;

  if (on && !_in_frame && _main_ui) {
    bool want = statusDotWanted();
    if (want != _dot_shown) {
      _dot_shown = want;
      paintStatusDot(want);
      flushFrame();
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

  flushFrame();
  _in_frame = false;
}

#endif
