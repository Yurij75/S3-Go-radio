#ifndef DISPLAY_WIDGETS_H
#define DISPLAY_WIDGETS_H

#include <Arduino_GFX_Library.h>

void initWidgetClock();
void createWidgetSprites();
void drawClockHMSS();
void drawVuBars();
void drawVuCardioWindowDynamic(bool advanceWaveform);
void updateVULevels();
void drawVuNeedleChannel(Arduino_Canvas* needleSprite, int posX, int posY, int vuLevel, bool mirrored);
bool setNeedleSpriteSettings(int width, int height, bool debugBorders);

#endif // DISPLAY_WIDGETS_H
