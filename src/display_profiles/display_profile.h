#pragma once

#if defined(DISPLAY_PROFILE_CUSTOM_GENERATED)
  #include "profile_custom_generated.h"
#elif defined(DISPLAY_ST7796)
  #include "profile_st7796.h"
#elif defined(DISPLAY_ILI9341)
  #include "profile_ili9341.h"
  #elif defined(DISPLAY_ILI9488)
  #include "profile_ili9488.h"
#elif defined(DISPLAY_NV3007)
  #include "profile_nv3007.h"
#elif defined(DISPLAY_ST7735_160x128)
  #include "profile_st7735_160x128.h"
#elif defined(DISPLAY_ST7789)
  #include "profile_st7789.h"
#elif defined(DISPLAY_ST7789_172)
  #include "profile_st7789_172.h"
#elif defined(DISPLAY_ST7789_76)
  #include "profile_st7789_76.h"
#else
  #error "No display profile selected"
#endif

#ifndef DISPLAY_PROFILE_ROTATION
  #define DISPLAY_PROFILE_ROTATION TFT_ROTATION
#endif
