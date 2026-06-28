#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <helpers/RefCountedDigitalPin.h>

class ST7735Display : public DisplayDriver {
#if defined(USE_PIN_TFT) && defined(ESP32_PLATFORM)
  // Hardware SPI on the HSPI host. The radio uses the default SPIClass host
  // (FSPI on ESP32-S3), so the two buses never collide even though the TFT
  // pins are routed through the GPIO matrix. Matches the proven ST7789LCD
  // pattern. Declared before `display` so it is constructed first.
  SPIClass spi_tft = SPIClass(HSPI);
#endif
  Adafruit_ST7735 display;
  bool _isOn;
  uint16_t _color;
  RefCountedDigitalPin* _peripher_power;

  bool i2c_probe(TwoWire& wire, uint8_t addr);
public:
#if defined(USE_PIN_TFT) && defined(ESP32_PLATFORM)
  // Hardware SPI: pins are remapped onto the FSPI host in begin(). This is
  // ~100x faster than bit-banged software SPI and makes setSPISpeed() effective.
  ST7735Display(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64),
      display(&spi_tft, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
      _peripher_power(peripher_power)
  {
    _isOn = false;
  }
#elif defined(USE_PIN_TFT)
  // Software (bit-banged) SPI fallback for non-ESP32 boards with custom TFT pins.
  ST7735Display(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64),
      display(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_SDA, PIN_TFT_SCL, PIN_TFT_RST),
      _peripher_power(peripher_power)
  {
    _isOn = false;
  }
#else
  ST7735Display(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64),
      display(&SPI1, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
      _peripher_power(peripher_power)
  {
    _isOn = false;
  }
#endif
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
