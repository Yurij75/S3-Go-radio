#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

void initClock();
void startRadio();
void createSprites();
void loadBackground();
void drawVolumeDisplay();
void displayTask(void* param);
void redrawScreen();
void redrawScreenLocked();
void refreshVuOnly();
void setDisplayTaskPaused(bool paused);
bool setNeedleSpriteSettings(int width, int height, bool debugBorders);

#endif // DISPLAY_MANAGER_H
