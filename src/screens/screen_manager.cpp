#include "screen_manager.h"

#include "screens.h"

namespace {
constexpr uint32_t kDefaultScreenDurationMs = 5000;
constexpr size_t kDefaultScreenIndex = 0;

ScreenState g_screenSequence[] = {
  {ScreenId::Clock1, true, kDefaultScreenDurationMs},
  {ScreenId::Clock2, true, kDefaultScreenDurationMs},
  {ScreenId::VuMeter1, true, kDefaultScreenDurationMs},
  {ScreenId::VuMeter2, true, kDefaultScreenDurationMs},
  {ScreenId::VuMeter3, true, kDefaultScreenDurationMs},
  {ScreenId::StationInfo, true, kDefaultScreenDurationMs},
};

size_t g_currentScreenIndex = kDefaultScreenIndex;

size_t screenCount() {
  return sizeof(g_screenSequence) / sizeof(g_screenSequence[0]);
}

int findScreenIndex(ScreenId id) {
  for (size_t i = 0; i < screenCount(); ++i) {
    if (g_screenSequence[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

bool moveWithStep(int step) {
  const size_t count = screenCount();
  if (count == 0) return false;

  for (size_t attempt = 0; attempt < count; ++attempt) {
    const size_t nextIndex = (g_currentScreenIndex + count + step) % count;
    g_currentScreenIndex = nextIndex;
    if (g_screenSequence[g_currentScreenIndex].enabled) {
      return true;
    }
  }
  return false;
}
}

void initScreenManager() {
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);

  for (size_t i = 0; i < count && i < screenCount(); ++i) {
    g_screenSequence[i].enabled = defs[i].enabledByDefault;
  }

  g_currentScreenIndex = kDefaultScreenIndex;
}

ScreenId getCurrentScreen() {
  return g_screenSequence[g_currentScreenIndex].id;
}

const ScreenState* getScreenSequence(size_t& count) {
  count = screenCount();
  return g_screenSequence;
}

bool setCurrentScreen(ScreenId id) {
  const int index = findScreenIndex(id);
  if (index < 0) return false;
  g_currentScreenIndex = static_cast<size_t>(index);
  return true;
}

bool setScreenEnabled(ScreenId id, bool enabled) {
  const int index = findScreenIndex(id);
  if (index < 0) return false;
  g_screenSequence[index].enabled = enabled;
  return true;
}

bool isScreenEnabled(ScreenId id) {
  const int index = findScreenIndex(id);
  if (index < 0) return false;
  return g_screenSequence[index].enabled;
}

bool setScreenDuration(ScreenId id, uint32_t durationMs) {
  const int index = findScreenIndex(id);
  if (index < 0) return false;
  g_screenSequence[index].durationMs = durationMs;
  return true;
}

uint32_t getScreenDuration(ScreenId id) {
  const int index = findScreenIndex(id);
  if (index < 0) return 0;
  return g_screenSequence[index].durationMs;
}

bool moveToNextScreen() {
  return moveWithStep(1);
}

bool moveToPreviousScreen() {
  return moveWithStep(-1);
}

size_t getCurrentScreenIndex() {
  return g_currentScreenIndex;
}
