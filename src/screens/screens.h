#ifndef SCREENS_H
#define SCREENS_H

#include <stddef.h>

#include "screen_types.h"

struct ScreenDefinition {
  ScreenId id;
  const char* key;
  const char* title;
  bool enabledByDefault;
  bool implemented;
};

const ScreenDefinition* getAllScreenDefinitions(size_t& count);
const ScreenDefinition* findScreenDefinition(ScreenId id);
const char* getScreenTitle(ScreenId id);
const char* getScreenKey(ScreenId id);
bool isClockScreen(ScreenId id);
bool isVuMeterScreen(ScreenId id);
bool isScreenImplemented(ScreenId id);

#endif // SCREENS_H
