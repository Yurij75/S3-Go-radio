// ============================================
// TFT Backlight (PWM) control
// ============================================

#include "backlight_control.h"

#include <Preferences.h>

#include "config.h"

namespace {
constexpr int kBacklightFreqHz = 20000;
constexpr int kBacklightResolutionBits = 8;  // 0..255
constexpr int kBacklightChannel = 7;
constexpr const char* kPrefsNs = "radio";
constexpr const char* kPrefsKey = "tftBrightness";

uint8_t g_brightness = TFT_BRIGHTNESS;
uint8_t g_lastNonZero = TFT_BRIGHTNESS;
bool g_initialized = false;

uint8_t maybeInvert(uint8_t duty) {
#if defined(TFT_BL_INVERTED) && (TFT_BL_INVERTED != 0)
  return uint8_t(255 - duty);
#else
  return duty;
#endif
}

void applyDuty(uint8_t duty) {
#if (TFT_BL >= 0)
  if (!g_initialized) return;
  ledcWrite(uint8_t(TFT_BL), maybeInvert(duty));
#else
  (void)duty;
#endif
}
}

void initTftBacklight() {
#if (TFT_BL >= 0)
  // Load stored value (if any).
  Preferences prefs;
  prefs.begin(kPrefsNs, true);
  int stored = prefs.getInt(kPrefsKey, -1);
  prefs.end();

  if (stored >= 0) g_brightness = uint8_t(constrain(stored, 0, 255));
  g_lastNonZero = (g_brightness == 0) ? uint8_t(TFT_BRIGHTNESS) : g_brightness;

  // Arduino-ESP32 v3 uses ledcAttach/ledcAttachChannel (no ledcSetup/ledcAttachPin).
  ledcAttachChannel(uint8_t(TFT_BL), kBacklightFreqHz, kBacklightResolutionBits, uint8_t(kBacklightChannel));
  g_initialized = true;
  applyDuty(g_brightness);
#endif
}

uint8_t getTftBrightness() { return g_brightness; }

void setTftBrightness(uint8_t duty, bool persist) {
  g_brightness = duty;
  if (duty != 0) g_lastNonZero = duty;
  applyDuty(duty);

  if (!persist) return;
  Preferences prefs;
  prefs.begin(kPrefsNs, false);
  prefs.putInt(kPrefsKey, int(duty));
  prefs.end();
}

void tftBacklightOff() { applyDuty(0); }

void tftBacklightOn() {
  // Restore last non-zero brightness, but respect if user wants it fully off.
  if (g_brightness == 0) {
    applyDuty(0);
    return;
  }
  applyDuty(g_lastNonZero);
}
