#ifdef ST7789

#include "ST7789Display.h"
#include "glcdfont6x8.h"
#ifdef CYRILLIC_SUPPORT
#include "OLEDDisplayFontsRU.h"
#endif

#ifndef X_OFFSET
#define X_OFFSET 0  // No offset needed for landscape
#endif

#ifndef Y_OFFSET
#define Y_OFFSET 1  // Vertical offset to prevent top row cutoff
#endif

#ifdef HELTEC_VISION_MASTER_T190
  #define SCALE_X  2.5f        // 320 / 128
  #define SCALE_Y  2.65625f    // 170 / 64
#else
  #define SCALE_X  1.875f      // 240 / 128
  #define SCALE_Y  1.875f      // uniform — 64 virtual → 120px, centered in 135px
  #undef  Y_OFFSET
  #define Y_OFFSET 7           // (135 - 120) / 2
#endif

bool ST7789Display::begin() {
  if(!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif
    digitalWrite(PIN_TFT_RST, HIGH);

    display.init();
    display.landscapeScreen();
    display.displayOn();
    setCursor(0,0);

    _isOn = true;
  }
  return true;
}

void ST7789Display::turnOn() {
  if (!_isOn) {
    // Restore power to the display but keep backlight off
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
    digitalWrite(PIN_TFT_RST, HIGH);
    
    // Re-initialize the display
    display.init();
    display.displayOn();
    delay(20);

    // Now turn on the backlight
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif    
    _isOn = true;
  }
}

void ST7789Display::turnOff() {
  digitalWrite(PIN_TFT_VDD_CTL, HIGH);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  digitalWrite(PIN_TFT_LEDA_CTL, !PIN_TFT_LEDA_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
#endif
  digitalWrite(PIN_TFT_RST, LOW);
  _isOn = false;
}

void ST7789Display::clear() {
  display.clear();
}

void ST7789Display::startFrame(Color bkg) {
  display.clear();
  _color = ST77XX_WHITE;
  _text_scale = 1;
  _use_v3_clock_font = false;
  display.setRGB(_color);
#ifdef CYRILLIC_SUPPORT
  display.setFont(ArialMT_Plain_16_RU);
#else
  display.setFont(ArialMT_Plain_16);
#endif
}

void ST7789Display::setTextSize(int sz) {
  _text_scale = 1;
  _use_v3_clock_font = false;
  switch(sz) {
    case 1 :
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_16_RU);
#else
      display.setFont(ArialMT_Plain_16);
#endif
      break;
    case 2 :
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_24_RU);
#else
      display.setFont(ArialMT_Plain_24);
#endif
      break;
    case 3 :
      _use_v3_clock_font = true;
      break;
    default:
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_16_RU);
#else
      display.setFont(ArialMT_Plain_16);
#endif
  }
}

void ST7789Display::setColor(Color c) {
  switch (c) {
    case DisplayDriver::DARK :
      _color = ST77XX_BLACK;
      display.setColor(OLEDDISPLAY_COLOR::BLACK);
      break;
#if 0
    case DisplayDriver::LIGHT :
      _color = ST77XX_WHITE;
      break;
    case DisplayDriver::RED :
      _color = ST77XX_RED;
      break;
    case DisplayDriver::GREEN :
      _color = ST77XX_GREEN;
      break;
    case DisplayDriver::BLUE :
      _color = ST77XX_BLUE;
      break;
    case DisplayDriver::YELLOW :
      _color = ST77XX_YELLOW;
      break;
    case DisplayDriver::ORANGE :
      _color = ST77XX_ORANGE;
      break;
#endif
    default:
      _color = ST77XX_WHITE;
      display.setColor(OLEDDISPLAY_COLOR::WHITE);
      break;
  }
  display.setRGB(_color);
}

void ST7789Display::setCursor(int x, int y) {
  _logical_x = x;
  _logical_y = y;
  _x = x*SCALE_X + X_OFFSET;
  _y = y*SCALE_Y + Y_OFFSET;
}

void ST7789Display::print(const char* str) {
  if (_use_v3_clock_font) {
    drawV3ClockString(_logical_x, _logical_y, str);
  } else if (_text_scale > 1) {
    drawScaledString(_x, _y, str);
  } else {
    display.drawString(_x, _y, str);
  }
}

void ST7789Display::printWordWrap(const char* str, int max_width) {
  display.drawStringMaxWidth(_x, _y, max_width*SCALE_X, str);
}

void ST7789Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  // Calculate the base position in display coordinates
  uint16_t startX = x * SCALE_X + X_OFFSET;
  uint16_t startY = y * SCALE_Y + Y_OFFSET;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * SCALE_Y);
    int y2 = startY + (int)((by + 1) * SCALE_Y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * SCALE_X);
      int x2 = startX + (int)((bx + 1) * SCALE_X);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h);
      }
    }
  }
}

uint16_t ST7789Display::getTextWidth(const char* str) {
  if (_use_v3_clock_font) return getV3ClockTextWidth(str);
  uint16_t phys_width = (_text_scale > 1) ? getScaledTextWidth(str) : display.getStringWidth(str);
  return phys_width / SCALE_X;
}

void ST7789Display::drawTextCentered(int mid_x, int y, const char* str) {
  if (_use_v3_clock_font) {
    int w = getV3ClockTextWidth(str);
    drawV3ClockString(mid_x - w / 2, y, str);
    return;
  }
  int phys_mid_x = mid_x * SCALE_X + X_OFFSET;
  int phys_y = y * SCALE_Y + Y_OFFSET;
  int phys_w = (_text_scale > 1) ? getScaledTextWidth(str) : display.getStringWidth(str);
  if (_text_scale > 1) {
    drawScaledString(phys_mid_x - phys_w / 2, phys_y, str);
  } else {
    display.drawString(phys_mid_x - phys_w / 2, phys_y, str);
  }
}

void ST7789Display::endFrame() {
  display.display();
}

uint16_t ST7789Display::getScaledTextWidth(const char* str) const {
  const uint8_t* fontData = display.getFontData();
  if (fontData == nullptr || str == nullptr) return 0;

  uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint8_t charCount = pgm_read_byte(fontData + CHAR_NUM_POS);
  uint16_t width = 0;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code >= firstChar && code < firstChar + charCount) {
      width += pgm_read_byte(fontData + JUMPTABLE_START +
                             (code - firstChar) * JUMPTABLE_BYTES +
                             JUMPTABLE_WIDTH) * _text_scale;
    }
  }
  return width;
}

void ST7789Display::drawScaledString(int x, int y, const char* str) {
  const uint8_t* fontData = display.getFontData();
  if (fontData == nullptr || str == nullptr) return;

  uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
  uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint8_t charCount = pgm_read_byte(fontData + CHAR_NUM_POS);
  uint16_t jumpTableSize = charCount * JUMPTABLE_BYTES;
  uint8_t rasterHeight = 1 + ((textHeight - 1) >> 3);
  int cursorX = 0;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code < firstChar || code >= firstChar + charCount) continue;

    uint8_t charCode = code - firstChar;
    uint16_t jump = ((uint16_t)pgm_read_byte(fontData + JUMPTABLE_START +
                                             charCode * JUMPTABLE_BYTES) << 8) |
                    pgm_read_byte(fontData + JUMPTABLE_START +
                                  charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);
    uint8_t byteSize = pgm_read_byte(fontData + JUMPTABLE_START +
                                     charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);
    uint8_t charWidth = pgm_read_byte(fontData + JUMPTABLE_START +
                                      charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

    if (jump != 0xFFFF) {
      uint16_t charData = JUMPTABLE_START + jumpTableSize + jump;
      for (uint8_t col = 0; col < charWidth; col++) {
        for (uint8_t row = 0; row < textHeight; row++) {
          uint16_t byteIndex = col * rasterHeight + (row >> 3);
          if (byteIndex >= byteSize) continue;
          uint8_t bits = pgm_read_byte(fontData + charData + byteIndex);
          if (bits & (1 << (row & 7))) {
            display.fillRect(x + cursorX + col * _text_scale,
                             y + row * _text_scale,
                             _text_scale,
                             _text_scale);
          }
        }
      }
    }
    cursorX += charWidth * _text_scale;
  }
}

uint16_t ST7789Display::getV3ClockTextWidth(const char* str) const {
  return str == nullptr ? 0 : strlen(str) * 6 * 3;
}

void ST7789Display::drawV3ClockString(int x, int y, const char* str) {
  if (str == nullptr) return;

#ifdef HELTEC_VISION_MASTER_T190
  const float clockScaleX = 320.0f / 128.0f;
  const float clockScaleY = 170.0f / 64.0f;
#else
  const float clockScaleX = 240.0f / 128.0f;
  const float clockScaleY = 135.0f / 64.0f;
#endif
  const int textSize = 3;
  int cursorX = x;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code < 32) code = 32;
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 6; col++) {
        uint16_t bit = row * 6 + col;
        uint8_t packed = pgm_read_byte(glcdfont6x8Bitmaps + (code - 32) * 6 + (bit >> 3));
        if ((packed & (0x80 >> (bit & 7))) == 0) continue;

        for (int sy = 0; sy < textSize; sy++) {
          for (int sx = 0; sx < textSize; sx++) {
            int vx = cursorX + col * textSize + sx;
            int vy = y + row * textSize + sy;
            int x1 = (int)(vx * clockScaleX);
            int x2 = (int)((vx + 1) * clockScaleX);
            int y1 = (int)(vy * clockScaleY);
            int y2 = (int)((vy + 1) * clockScaleY);
            int blockW = x2 - x1;
            int blockH = y2 - y1;
            if (blockW < 1) blockW = 1;
            if (blockH < 1) blockH = 1;
            display.fillRect(x1, y1, blockW, blockH);
          }
        }
      }
    }
    cursorX += 6 * textSize;
  }
}

#endif
