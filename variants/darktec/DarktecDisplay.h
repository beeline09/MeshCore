#pragma once

#include <helpers/ui/SSD1306Display.h>

// OLED Darktec: поверх кадра ui-new дорисовывает молнию на иконке батареи.
class DarktecDisplay : public SSD1306Display {
public:
  void endFrame() override;
};
