#pragma once

#include <helpers/ui/SSD1306Display.h>

// OLED Darktec: молния на батарее при зарядке + точка-пульс вместо LED.
class DarktecDisplay : public SSD1306Display {
  bool _in_frame = false;
  bool _dot_shown = false;
  bool _was_charging = false;
  bool _main_ui = false;

  bool statusDotWanted() const;
  void paintStatusDot(bool on);

public:
  bool isOn() override;
  void turnOff() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void drawRect(int x, int y, int w, int h) override;
  void endFrame() override;

private:
  // spec 0002 FR-3: true, когда настройка разрешает экрану гаснуть при зарядке.
  bool screenSleepAllowed() const;
};
