#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "screen_types.h"

struct ScreenState {
  ScreenId id;
  bool enabled;
  uint32_t durationMs;
};

void initScreenManager();
ScreenId getCurrentScreen();
const ScreenState* getScreenSequence(size_t& count);
bool setCurrentScreen(ScreenId id);
bool setScreenEnabled(ScreenId id, bool enabled);
bool isScreenEnabled(ScreenId id);
bool setScreenDuration(ScreenId id, uint32_t durationMs);
uint32_t getScreenDuration(ScreenId id);
bool moveToNextScreen();
bool moveToPreviousScreen();
size_t getCurrentScreenIndex();

#endif // SCREEN_MANAGER_H
