#ifndef SCREEN_TYPES_H
#define SCREEN_TYPES_H

#include <stdint.h>

enum class ScreenId : uint8_t {
  Clock1 = 0,
  Clock2,
  VuMeter1,
  VuMeter2,
  VuMeter3,
  StationInfo,
  Count
};

#endif // SCREEN_TYPES_H
