// ============================================
// TFT Backlight (PWM) control
// ============================================

#ifndef BACKLIGHT_CONTROL_H
#define BACKLIGHT_CONTROL_H

#include <Arduino.h>

// Initializes PWM and applies stored brightness (Preferences key: "tftBrightness").
void initTftBacklight();

// Returns last requested brightness (0-255).
uint8_t getTftBrightness();

// Sets brightness (0-255). If persist=true, stores to Preferences.
void setTftBrightness(uint8_t duty, bool persist);

// Temporary power-style controls (do not persist).
void tftBacklightOff();
void tftBacklightOn();

#endif // BACKLIGHT_CONTROL_H

