#pragma once

#include <helpers/ui/SSD1306Display.h>

// OLED overlays: LoRa activity dot on home, wake display when pack charging starts.
class DarktecDisplay : public SSD1306Display {
  bool _in_frame = false;
  bool _dot_shown = false;
  bool _was_charging = false;
  bool _main_ui = false;

  bool statusDotWanted() const;
  void paintStatusDot(bool on);

public:
  bool isOn() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void drawRect(int x, int y, int w, int h) override;
  void endFrame() override;
};
