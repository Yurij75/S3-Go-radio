#include "screens.h"

namespace {
constexpr ScreenDefinition kScreenDefinitions[] = {
  {ScreenId::Clock1, "clock1", "Clock 1", true, true},
  {ScreenId::Clock2, "clock2", "Clock 2", true, true},
  {ScreenId::VuMeter1, "vumeter1", "VU Meter 1", true, true},
  {ScreenId::VuMeter2, "vumeter2", "VU Meter 2", true, true},
  {ScreenId::VuMeter3, "vumeter3", "VU Meter 3", true, true},
  {ScreenId::StationInfo, "station_info", "Station Info", true, true},
};
}

const ScreenDefinition* getAllScreenDefinitions(size_t& count) {
  count = sizeof(kScreenDefinitions) / sizeof(kScreenDefinitions[0]);
  return kScreenDefinitions;
}

const ScreenDefinition* findScreenDefinition(ScreenId id) {
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);
  for (size_t i = 0; i < count; ++i) {
    if (defs[i].id == id) return &defs[i];
  }
  return nullptr;
}

const char* getScreenTitle(ScreenId id) {
  const ScreenDefinition* def = findScreenDefinition(id);
  return def ? def->title : "Unknown";
}

const char* getScreenKey(ScreenId id) {
  const ScreenDefinition* def = findScreenDefinition(id);
  return def ? def->key : "unknown";
}

bool isClockScreen(ScreenId id) {
  return id == ScreenId::Clock1 || id == ScreenId::Clock2;
}

bool isVuMeterScreen(ScreenId id) {
  return id == ScreenId::VuMeter1 || id == ScreenId::VuMeter2 || id == ScreenId::VuMeter3;
}

bool isScreenImplemented(ScreenId id) {
  const ScreenDefinition* def = findScreenDefinition(id);
  return def ? def->implemented : false;
}
