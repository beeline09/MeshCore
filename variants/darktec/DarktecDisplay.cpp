#ifdef DISPLAY_CLASS

#include "DarktecDisplay.h"
#include "DarktecBoard.h"

extern DarktecBoard board;

void DarktecDisplay::endFrame() {
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

  SSD1306Display::endFrame();
}

#endif
