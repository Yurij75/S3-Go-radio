#include "display_widgets.h"

#include <Arduino.h>
#include <Audio.h>
#include <cmath>
#include <vector>

#include "config.h"
#include "psram_canvas.h"
#include "screens/screen_manager.h"

extern Audio audio;
extern Arduino_GFX* gfx;
extern Arduino_Canvas* bgSprite;
extern Arduino_Canvas* needleSpriteL;
extern Arduino_Canvas* needleSpriteR;
extern Arduino_Canvas* clockSprite;
extern Arduino_Canvas* analogClockSprite;
extern SemaphoreHandle_t xMutex;

extern int vuWidth;
extern int vuHeight;
extern int vuBarsHeight;
extern int vuBarsSegments;
extern int vuBarsOffsetX;
extern int vuBarsOffsetY;
extern int vuCardioX;
extern int vuCardioY;
extern int vuCardioW;
extern int vuCardioH;
extern int clockW;
extern int clockH;
extern int analogClockW;
extern int analogClockH;
extern int clockX;
extern int clockY;
extern int clockSpriteX;
extern int clockSpriteY;
extern int current_needle_L;
extern int target_needle_L;
extern int current_needle_R;
extern int target_needle_R;
extern int needlePosLX;
extern int needlePosLY;
extern int needlePosRX;
extern int needlePosRY;
extern int needleUpSpeed;
extern int needleDownSpeed;
extern int needleThickness;
extern int needleLenMain;
extern int needleLenRed;
extern int needleCY;
extern int needleRotation;
extern int needleSpriteWidth;
extern int needleSpriteHeight;
extern int needleMinAngle;
extern int needleMaxAngle;
extern uint16_t colorClock;
extern uint16_t colorNeedleMain;
extern uint16_t colorNeedleRed;
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern bool radioStarted;
extern bool isPaused;
extern bool redrawClock;

namespace {

Arduino_Canvas* g_vuBarsSprite = nullptr;
Arduino_Canvas* g_vuCardioSprite = nullptr;
std::vector<int16_t> g_vuCardioWaveform;

float g_vuNeedleLevelL = 0.0f;
float g_vuNeedleLevelR = 0.0f;
unsigned long g_vuNeedleLastMs = 0;
float g_vuCardioAutoGain = 1.0f;
float g_vuCardioPeak = 0.0f;
float g_vuCardioRms = 0.0f;

bool shouldShowClock(ScreenId screen) {
  return screen == ScreenId::Clock1 || screen == ScreenId::Clock2;
}

int analogClockDiameter() {
  return static_cast<int>(min(vuWidth, vuHeight) * 0.62f);
}

void copyBackgroundToSprite(Arduino_Canvas* sprite, int spriteW, int spriteH, int drawX, int drawY) {
  if (!sprite || !bgSprite) return;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* spriteBuf = (uint16_t*)sprite->getFramebuffer();
  if (!bgBuf || !spriteBuf) return;

  for (int y = 0; y < spriteH; y++) {
    const int srcY = drawY + y;
    if (srcY >= 0 && srcY < vuHeight) {
      if (drawX >= 0 && drawX + spriteW <= vuWidth) {
        memcpy(&spriteBuf[y * spriteW], &bgBuf[srcY * vuWidth + drawX], spriteW * sizeof(uint16_t));
      } else {
        for (int x = 0; x < spriteW; ++x) {
          const int srcX = drawX + x;
          spriteBuf[y * spriteW + x] = (srcX >= 0 && srcX < vuWidth) ? bgBuf[srcY * vuWidth + srcX] : RGB565_BLACK;
        }
      }
    } else {
      memset(&spriteBuf[y * spriteW], 0, spriteW * sizeof(uint16_t));
    }
  }
}

void ensureVuBarsSprite(int width, int height) {
  if (!gfx || width <= 0 || height <= 0) return;
  if (g_vuBarsSprite &&
      g_vuBarsSprite->width() == width &&
      g_vuBarsSprite->height() == height) {
    return;
  }

  if (g_vuBarsSprite) {
    delete g_vuBarsSprite;
    g_vuBarsSprite = nullptr;
  }

  g_vuBarsSprite = createManagedCanvas(width, height, gfx, true, "vuBarsSprite");
}

bool ensureVuCardioSprite(int width, int height) {
  width = max(1, width);
  height = max(1, height);
  if (g_vuCardioSprite &&
      g_vuCardioSprite->width() == width &&
      g_vuCardioSprite->height() == height) {
    return true;
  }

  if (g_vuCardioSprite) {
    delete g_vuCardioSprite;
    g_vuCardioSprite = nullptr;
  }

  g_vuCardioSprite = createManagedCanvas(width, height, gfx, true, "vuCardioSprite");
  if (!g_vuCardioSprite) {
    g_vuCardioWaveform.clear();
    return false;
  }

  g_vuCardioWaveform.assign(width, height / 2);
  return true;
}

}  // namespace

void initWidgetClock() {
  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
}

void drawClockHMSS() {
  const ScreenId currentScreen = getCurrentScreen();
  if (!shouldShowClock(currentScreen)) return;

  static int lastSec = -1;
  static unsigned long lastBlinkCheck = 0;
  const bool forceRedraw = redrawClock;

  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  const unsigned long blinkCheck = BLINK_COLON ? (millis() / 500) : (millis() / 1000);
  if (!forceRedraw && timeinfo.tm_sec == lastSec && blinkCheck == lastBlinkCheck) return;

  if (timeinfo.tm_sec != lastSec) lastSec = timeinfo.tm_sec;
  lastBlinkCheck = blinkCheck;

  char clockBuf[10];
  if (SHOW_SECONDS) {
    sprintf(clockBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    sprintf(clockBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }

  if (currentScreen == ScreenId::Clock2) {
    copyBackgroundToSprite(analogClockSprite, analogClockW, analogClockH, clockX, clockY);

    const int radius = analogClockDiameter() / 2;
    const int cx = analogClockW / 2;
    const int cy = analogClockH / 2;
    const float secondAngle = (timeinfo.tm_sec * 6.0f - 90.0f) * 3.14159f / 180.0f;
    const float minuteAngle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f - 90.0f) * 3.14159f / 180.0f;
    const float hourAngle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f - 90.0f) * 3.14159f / 180.0f;

    const int secLen = max(8, radius - 2);
    const int minLen = static_cast<int>(secLen * 0.78f);
    const int hourLen = static_cast<int>(secLen * 0.58f);

    for (int t = -2; t <= 2; ++t) {
      analogClockSprite->drawLine(cx + t, cy, cx + cos(hourAngle) * hourLen + t, cy + sin(hourAngle) * hourLen, colorClock);
    }
    for (int t = -1; t <= 1; ++t) {
      analogClockSprite->drawLine(cx + t, cy, cx + cos(minuteAngle) * minLen + t, cy + sin(minuteAngle) * minLen, colorClock);
    }
    analogClockSprite->drawLine(cx, cy, cx + cos(secondAngle) * secLen, cy + sin(secondAngle) * secLen, colorNeedleRed);
    analogClockSprite->fillCircle(cx, cy, 5, colorClock);
    analogClockSprite->fillCircle(cx, cy, 2, colorNeedleRed);
    gfx->draw16bitRGBBitmap(clockX, clockY, (uint16_t*)analogClockSprite->getFramebuffer(), analogClockW, analogClockH);
    redrawClock = false;
    return;
  }

  copyBackgroundToSprite(clockSprite, clockW, clockH, clockSpriteX, clockY);
  clockSprite->setTextColor(colorClock);
  clockSprite->setFont(&CLOCK_FONT);

  char timeDisplay[10];
  if (BLINK_COLON && (millis() / 500) % 2 != 0) {
    if (SHOW_SECONDS) {
      sprintf(timeDisplay, "%02d;%02d;%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      sprintf(timeDisplay, "%02d;%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
  } else {
    sprintf(timeDisplay, "%s", clockBuf);
  }

  int16_t x = 0;
  int16_t y = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  clockSprite->getTextBounds(timeDisplay, 0, 0, &x, &y, &w, &h);

  const int drawX = (clockW - w) / 2;
  const int drawY = (clockH + h) / 2;
  clockSprite->setCursor(drawX, drawY);
  clockSprite->print(timeDisplay);

  gfx->draw16bitRGBBitmap(clockSpriteX, clockY, (uint16_t*)clockSprite->getFramebuffer(), clockW, clockH);
  redrawClock = false;
}

void drawVuBars() {
  if (!gfx || !bgSprite) return;

  const int canvasW = max(1, min(vuWidth, static_cast<int>(gfx->width())));
  const int canvasH = max(1, min(vuHeight, static_cast<int>(gfx->height())));
  const int marginX = max(8, canvasW / 20);
  const int marginY = max(8, canvasH / 20);
  const int gapY = max(8, canvasH / 20);
  const int outerW = max(40, canvasW - marginX * 2);
  const int outerH = constrain(vuBarsHeight, 6, max(8, (canvasH - marginY * 2 - gapY) / 2));
  const int centeredX = (canvasW - outerW) / 2;
  const int centeredY = (canvasH - (outerH * 2 + gapY)) / 2;
  const int drawX = constrain(centeredX + vuBarsOffsetX, 0, max(0, canvasW - outerW));
  const int topY = constrain(centeredY + vuBarsOffsetY, 0, max(0, canvasH - (outerH * 2 + gapY)));
  const int blockY = topY;
  const int blockH = outerH * 2 + gapY;

  const int innerPad = 2;
  const int innerX = drawX + innerPad;
  const int innerW = max(1, outerW - innerPad * 2);
  const int innerH = max(1, outerH - innerPad * 2);
  const int segments = constrain(vuBarsSegments, 4, 40);
  const int segGap = max(2, innerW / 120);
  const int segW = max(1, (innerW - segGap * (segments - 1)) / segments);

  ensureVuBarsSprite(canvasW, canvasH);
  if (!g_vuBarsSprite) return;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* spriteBuf = (uint16_t*)g_vuBarsSprite->getFramebuffer();
  if (!bgBuf || !spriteBuf) return;

  for (int y = 0; y < blockH; ++y) {
    const int srcY = blockY + y;
    if (srcY < 0 || srcY >= canvasH) {
      memset(&spriteBuf[y * canvasW], 0, canvasW * sizeof(uint16_t));
    } else {
      memcpy(&spriteBuf[y * canvasW], &bgBuf[srcY * vuWidth], canvasW * sizeof(uint16_t));
    }
  }

  auto segmentColor = [&](int index) -> uint16_t {
    const float ratio = static_cast<float>(index + 1) / static_cast<float>(segments);
    if (ratio <= 0.65f) return RGB565_GREEN;
    if (ratio <= 0.85f) return RGB565_YELLOW;
    return RGB565_RED;
  };

  auto drawSegmentedBar = [&](int level, int yInBlock) {
    const int activeSegments = map(constrain(level, 0, 100), 0, 100, 0, segments);
    g_vuBarsSprite->fillRect(innerX, yInBlock + innerPad, innerW, innerH, RGB565_BLACK);

    for (int i = 0; i < segments; ++i) {
      const int sx = innerX + i * (segW + segGap);
      const int sy = yInBlock + innerPad;
      if (i < activeSegments) {
        g_vuBarsSprite->fillRect(sx, sy, segW, innerH, segmentColor(i));
      } else {
        g_vuBarsSprite->drawRect(sx, sy, segW, innerH, RGB565_GRAY);
      }
    }
  };

  drawSegmentedBar(current_needle_L, 0);
  drawSegmentedBar(current_needle_R, outerH + gapY);

  gfx->draw16bitRGBBitmap(0, blockY, (uint16_t*)g_vuBarsSprite->getFramebuffer(), canvasW, blockH);
}

void drawVuCardioWindowDynamic(bool advanceWaveform) {
  if (!gfx || !bgSprite) return;

  const int screenW = max(1, min(vuWidth, static_cast<int>(gfx->width())));
  const int screenH = max(1, min(vuHeight, static_cast<int>(gfx->height())));
  const int drawW = constrain(vuCardioW, 80, screenW);
  const int drawH = constrain(vuCardioH, 40, screenH);
  const int drawX = constrain(vuCardioX, 0, max(0, screenW - drawW));
  const int drawY = constrain(vuCardioY, 0, max(0, screenH - drawH));

  if (!ensureVuCardioSprite(drawW, drawH) || !g_vuCardioSprite) return;

  copyBackgroundToSprite(g_vuCardioSprite, drawW, drawH, drawX, drawY);
  uint16_t* spriteBuf = (uint16_t*)g_vuCardioSprite->getFramebuffer();
  if (!spriteBuf) return;

  const int centerY = drawH / 2;
  const int innerPad = max(6, min(drawW, drawH) / 18);
  const int usableH = max(8, drawH - innerPad * 2);

  if (advanceWaveform && !g_vuCardioWaveform.empty()) {
    float levelNorm = 0.0f;
    if (radioStarted && !isPaused) {
      const int vu = audio.getVUlevel();
      const float left = ((vu >> 8) & 0xFF) / 255.0f;
      const float right = (vu & 0xFF) / 255.0f;
      const float mono = (left + right) * 0.5f;

      g_vuCardioPeak = max(mono, g_vuCardioPeak * 0.92f);
      g_vuCardioRms = g_vuCardioRms * 0.88f + (mono * mono) * 0.12f;

      float gain = 1.0f;
      const float rmsRoot = sqrtf(max(0.0f, g_vuCardioRms));
      if (rmsRoot > 0.035f) {
        gain = 0.48f / rmsRoot;
      }
      if (g_vuCardioPeak > 0.90f) {
        gain *= 0.74f;
      }

      gain = constrain(gain, 0.55f, 2.4f);
      g_vuCardioAutoGain = g_vuCardioAutoGain * 0.90f + gain * 0.10f;
      levelNorm = constrain(mono * g_vuCardioAutoGain, 0.0f, 1.0f);
    }

    float shapedLevel = levelNorm;
    if (shapedLevel > 0.68f) {
      shapedLevel = 0.68f + (shapedLevel - 0.68f) * 0.32f;
    }
    shapedLevel = pow(shapedLevel, 0.90f);

    const float basePulse = (radioStarted && !isPaused) ? min(shapedLevel, 0.82f) : 0.03f;
    const float jitterRange = (radioStarted && !isPaused) ? (0.05f + shapedLevel * 0.07f) : 0.03f;
    const float jitter = (static_cast<float>(random(-100, 101)) / 100.0f) * jitterRange;
    const float pulse = constrain(basePulse * 1.08f + jitter, 0.0f, 0.90f);
    const int amplitude = max(2, static_cast<int>(pulse * (usableH * 0.42f)));
    const int spikeBias = (radioStarted && !isPaused && pulse > 0.28f) ? max(0, amplitude / 3) : 0;

    int newPoint = centerY - amplitude - spikeBias + random(-2, 3);
    newPoint = constrain(newPoint, innerPad, drawH - innerPad - 1);

    for (size_t i = 1; i < g_vuCardioWaveform.size(); ++i) {
      g_vuCardioWaveform[i - 1] = g_vuCardioWaveform[i];
    }
    g_vuCardioWaveform.back() = static_cast<int16_t>(newPoint);
  }

  if (g_vuCardioWaveform.size() != static_cast<size_t>(drawW)) {
    g_vuCardioWaveform.assign(drawW, centerY);
  }

  const uint16_t glowColor = RGB565(70, 255, 70);
  const uint16_t traceColor = RGB565(62, 255, 62);
  const uint16_t dimTrace = RGB565(30, 120, 30);

  for (int x = 1; x < drawW; ++x) {
    const int y0 = constrain(g_vuCardioWaveform[x - 1], 0, drawH - 1);
    const int y1 = constrain(g_vuCardioWaveform[x], 0, drawH - 1);
    const int mirrorY0 = constrain(centerY + (centerY - y0), 0, drawH - 1);
    const int mirrorY1 = constrain(centerY + (centerY - y1), 0, drawH - 1);

    g_vuCardioSprite->drawLine(x - 1, min(drawH - 1, y0 + 1), x, min(drawH - 1, y1 + 1), dimTrace);
    g_vuCardioSprite->drawLine(x - 1, y0, x, y1, glowColor);
    g_vuCardioSprite->drawLine(x - 1, max(0, mirrorY0 - 1), x, max(0, mirrorY1 - 1), dimTrace);
    g_vuCardioSprite->drawLine(x - 1, mirrorY0, x, mirrorY1, glowColor);
    if ((x % 2) == 0) {
      g_vuCardioSprite->drawLine(x - 1, max(0, y0 - 1), x, max(0, y1 - 1), traceColor);
      g_vuCardioSprite->drawLine(x - 1, min(drawH - 1, mirrorY0 + 1), x, min(drawH - 1, mirrorY1 + 1), traceColor);
    }
  }

  gfx->draw16bitRGBBitmap(drawX, drawY, spriteBuf, drawW, drawH);
}

void updateVULevels() {
  const unsigned long now = millis();
  if (g_vuNeedleLastMs == 0) g_vuNeedleLastMs = now;

  float dt = static_cast<float>(now - g_vuNeedleLastMs) / 1000.0f;
  g_vuNeedleLastMs = now;
  dt = constrain(dt, 0.001f, 0.08f);

  float targetL = 0.0f;
  float targetR = 0.0f;
  if (radioStarted && !isPaused) {
    const int vu = audio.getVUlevel();
    const uint8_t left = (vu >> 8) & 0xFF;
    const uint8_t right = vu & 0xFF;
    targetL = pow(left / 255.0f, 0.7f) * 100.0f;
    targetR = pow(right / 255.0f, 0.7f) * 100.0f;
  }

  target_needle_L = static_cast<int>(targetL);
  target_needle_R = static_cast<int>(targetR);

  const float attackPerSecond = 220.0f + constrain(needleUpSpeed, 1, 20) * 55.0f;
  const float releasePerSecond = 18.0f + constrain(needleDownSpeed, 1, 20) * 14.0f;

  auto updateNeedle = [&](float target, float& current) {
    if (target >= current) {
      current = min(target, current + attackPerSecond * dt);
    } else {
      current = max(target, current - releasePerSecond * dt);
    }
    if (target < 1.0f && current < 0.35f) {
      current = 0.0f;
    }
  };

  updateNeedle(targetL, g_vuNeedleLevelL);
  updateNeedle(targetR, g_vuNeedleLevelR);

  current_needle_L = constrain(static_cast<int>(g_vuNeedleLevelL + 0.5f), 0, 100);
  current_needle_R = constrain(static_cast<int>(g_vuNeedleLevelR + 0.5f), 0, 100);
}

void createWidgetSprites() {
  vuWidth = gfx->width();
  vuHeight = gfx->height();

  const int spriteW = constrain(needleSpriteWidth, 16, vuWidth);
  const int spriteH = constrain(needleSpriteHeight, 16, vuHeight);
  needleSpriteWidth = spriteW;
  needleSpriteHeight = spriteH;

  bgSprite = createManagedCanvas(vuWidth, vuHeight, gfx, true, "bgSprite");
  needleSpriteL = createManagedCanvas(spriteW, spriteH, gfx, false, "needleSpriteL");
  needleSpriteR = createManagedCanvas(spriteW, spriteH, gfx, false, "needleSpriteR");

  gfx->setFont(&CLOCK_FONT);
  char testStr[] = "88:88:88";
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  gfx->getTextBounds(testStr, 0, 0, &x1, &y1, &w, &h);
  clockW = w + 6;
  clockH = h + 6;
  analogClockW = analogClockDiameter() + 2;
  analogClockH = analogClockDiameter() + 2;

  clockSprite = createManagedCanvas(clockW, clockH, gfx, true, "clockSprite");
  analogClockSprite = createManagedCanvas(analogClockW, analogClockH, gfx, true, "analogClockSprite");
  if (!bgSprite || !needleSpriteL || !needleSpriteR || !clockSprite || !analogClockSprite) {
    Serial.println("Sprite allocation failed");
    return;
  }

  clockSpriteX = clockX;
  clockSpriteY = clockY;
  clockX = clockSpriteX;
  clockY = clockSpriteY;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* clkBuf = (uint16_t*)clockSprite->getFramebuffer();
  for (int y = 0; y < clockH; y++) {
    const int srcY = clockSpriteY + y;
    if (srcY >= 0 && srcY < vuHeight) {
      memcpy(&clkBuf[y * clockW], &bgBuf[srcY * vuWidth + clockSpriteX], clockW * sizeof(uint16_t));
    } else {
      memset(&clkBuf[y * clockW], 0, clockW * sizeof(uint16_t));
    }
  }

  uint16_t* analogBuf = (uint16_t*)analogClockSprite->getFramebuffer();
  for (int y = 0; y < analogClockH; y++) {
    memset(&analogBuf[y * analogClockW], 0, analogClockW * sizeof(uint16_t));
  }
}

void drawVuNeedleChannel(Arduino_Canvas* needleSprite, int posX, int posY, int vuLevel, bool mirrored) {
  if (!needleSprite) return;

  const int spriteW = needleSprite->width();
  const int spriteH = needleSprite->height();
  uint16_t* needleBuf = (uint16_t*)needleSprite->getFramebuffer();
  copyBackgroundToSprite(needleSprite, spriteW, spriteH, posX, posY);

  const int cx = spriteW / 2;
  const int cy = needleCY;
  const float angle = map(vuLevel, 0, 100, needleMinAngle, needleMaxAngle);
  const float rad = angle * 3.14159f / 180.0f;
  const float cosA = cos(rad);
  const float sinA = sin(rad);
  const float originX = (spriteW - 1) * 0.5f;
  const float originY = (spriteH - 1) * 0.5f;
  const int rotation = ((needleRotation % 360) + 360) % 360;

  auto transformPoint = [=](int x, int y, int& outX, int& outY) {
    const float px = static_cast<float>(x);
    const float py = static_cast<float>(mirrored ? (spriteH - 1 - y) : y);
    const float dx = px - originX;
    const float dy = py - originY;
    float rx = dx;
    float ry = dy;

    switch (rotation) {
      case 90:
        rx = -dy;
        ry = dx;
        break;
      case 180:
        rx = -dx;
        ry = -dy;
        break;
      case 270:
        rx = dy;
        ry = -dx;
        break;
      default:
        break;
    }

    outX = static_cast<int>(roundf(originX + rx));
    outY = static_cast<int>(roundf(originY + ry));
  };

  for (int t = -needleThickness / 2; t <= needleThickness / 2; t++) {
    const int offsetX = t * sin(rad);
    const int offsetY = -t * cos(rad);
    int x1, y1, x2, y2, x3, y3;
    transformPoint(cx + offsetX, cy + offsetY, x1, y1);
    transformPoint(cx + (needleLenMain - needleLenRed) * cosA + offsetX,
                   cy + (needleLenMain - needleLenRed) * sinA + offsetY, x2, y2);
    transformPoint(cx + needleLenMain * cosA + offsetX,
                   cy + needleLenMain * sinA + offsetY, x3, y3);
    needleSprite->drawLine(x1, y1, x2, y2, colorNeedleMain);
    needleSprite->drawLine(x2, y2, x3, y3, colorNeedleRed);
  }

  int pivotX;
  int pivotY;
  transformPoint(cx, cy, pivotX, pivotY);
  needleSprite->fillCircle(pivotX, pivotY, 3, RGB565(0xAD, 0x55, 0x55));
  needleSprite->fillCircle(pivotX, pivotY, 1, colorNeedleRed);
  gfx->draw16bitRGBBitmap(posX, posY, needleBuf, spriteW, spriteH);

  if (debugBordersEnabled) {
    gfx->drawRect(needlePosLX, needlePosLY, needleSpriteL->width(), needleSpriteL->height(), RGB565_RED);
    gfx->drawRect(needlePosRX, needlePosRY, needleSpriteR->width(), needleSpriteR->height(), RGB565_BLUE);
  }
}

bool setNeedleSpriteSettings(int width, int height, bool debugBorders) {
  const int maxW = (vuWidth > 0) ? vuWidth : 480;
  const int maxH = (vuHeight > 0) ? vuHeight : 480;
  const int newW = constrain(width, 16, maxW);
  const int newH = constrain(height, 16, maxH);

  debugBordersEnabled = debugBorders;
  needleSpriteWidth = newW;
  needleSpriteHeight = newH;

  if (needleSpriteL == nullptr || needleSpriteR == nullptr) {
    return true;
  }
  if (needleSpriteL->width() == newW && needleSpriteL->height() == newH &&
      needleSpriteR->width() == newW && needleSpriteR->height() == newH) {
    return true;
  }
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return false;
  }

  Arduino_Canvas* newLeft = createManagedCanvas(newW, newH, gfx, false, "needleSpriteL");
  Arduino_Canvas* newRight = createManagedCanvas(newW, newH, gfx, false, "needleSpriteR");
  if (!newLeft || !newRight) {
    if (newLeft) delete newLeft;
    if (newRight) delete newRight;
    xSemaphoreGive(xMutex);
    return false;
  }

  delete needleSpriteL;
  delete needleSpriteR;
  needleSpriteL = newLeft;
  needleSpriteR = newRight;

  xSemaphoreGive(xMutex);
  return true;
}
