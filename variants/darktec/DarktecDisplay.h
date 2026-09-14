#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>
#include <helpers/RefCountedDigitalPin.h>

#ifndef PIN_OLED_RESET
  #define PIN_OLED_RESET        21
#endif
#ifndef DISPLAY_ADDRESS
  #define DISPLAY_ADDRESS   0x3C
#endif

// Full OLED driver in the variant: official SSD1306Display keeps `display` private,
// so Cyrillic (GFXfont + UTF-8→CP1251) cannot be injected from a subclass.
class DarktecDisplay : public DisplayDriver {
  Adafruit_SSD1306 display;
  bool _isOn;
  uint8_t _color;
  RefCountedDigitalPin* _peripher_power;
  uint8_t _font_size = 1;
  int _cursor_y_raw = 0;

  bool _in_frame = false;
  bool _dot_shown = false;
  bool _was_charging = false;
  bool _main_ui = false;

  bool i2c_probe(TwoWire& wire, uint8_t addr);
  bool statusDotWanted() const;
  void paintStatusDot(bool on);
  void flushFrame();

public:
  DarktecDisplay(RefCountedDigitalPin* peripher_power=NULL)
    : DisplayDriver(128, 64),
      display(128, 64, &Wire, PIN_OLED_RESET),
      _peripher_power(peripher_power)
  {
    _isOn = false;
  }

  bool begin();
  bool isOn() override;
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void printWordWrap(const char* str, int max_width) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) override;
  void endFrame() override;
};
