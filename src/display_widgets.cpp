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
extern uint16_t vuBarsColorBottom;
extern uint16_t vuBarsColorTop;
extern int vuCardioX;
extern int vuCardioY;
extern int vuCardioW;
extern int vuCardioH;
extern int fftWindowX;
extern int fftWindowY;
extern int fftWindowW;
extern int fftWindowH;
extern int fftBands;
extern int fftSegments;
extern int fftMinBin;
extern int fftMaxBin;
extern int fftAggregation;
extern int fftCurve;
extern float fftGain;
extern float fftPreGain;
extern float fftNoiseGate;
extern float fftNoiseFloor;
extern float fftSensitivity;
extern float fftGamma;
extern float fftLogStrength;
extern float fftAttack;
extern float fftRelease;
extern float fftLowBoost;
extern float fftMidBoost;
extern float fftHighBoost;
extern int fftBarGap;
extern int fftSideMargin;
extern int fftTopMargin;
extern int fftBottomMargin;
extern int fftMinLitSegments;
extern bool fftPeakHold;
extern int fftPeakFall;
extern bool fftMirror;
extern bool fftInvert;
extern uint16_t fftColorBottom;
extern uint16_t fftColorTop;
extern uint16_t fftColorPeak;
extern uint16_t fftColorOff;
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
Arduino_Canvas* g_fftSprite = nullptr;
std::vector<int16_t> g_vuCardioWaveform;

float g_vuNeedleLevelL = 0.0f;
float g_vuNeedleLevelR = 0.0f;
unsigned long g_vuNeedleLastMs = 0;
float g_vuCardioAutoGain = 1.0f;
float g_vuCardioPeak = 0.0f;
float g_vuCardioRms = 0.0f;
float g_fftSmoothed[FFT_BANDS] = {0.0f};
float g_fftPeaks[FFT_BANDS] = {0.0f};

uint16_t blendRgb565(uint16_t a, uint16_t b, float ratio) {
  ratio = constrain(ratio, 0.0f, 1.0f);
  const int ar = ((a >> 11) & 0x1F) << 3;
  const int ag = ((a >> 5) & 0x3F) << 2;
  const int ab = (a & 0x1F) << 3;
  const int br = ((b >> 11) & 0x1F) << 3;
  const int bg = ((b >> 5) & 0x3F) << 2;
  const int bb = (b & 0x1F) << 3;
  return RGB565(ar + static_cast<int>((br - ar) * ratio + 0.5f),
                ag + static_cast<int>((bg - ag) * ratio + 0.5f),
                ab + static_cast<int>((bb - ab) * ratio + 0.5f));
}

float catmullRom(float p0, float p1, float p2, float p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                 (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

bool shouldShowClock(ScreenId screen) {
  return screen == ScreenId::Clock1 || screen == ScreenId::Clock2;
}

const char* clockMaxText() {
  return SHOW_SECONDS ? "88:88:88" : "88:88";
}

void measureClockText(Arduino_GFX* target, const char* text, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h) {
  *x = 0;
  *y = 0;
  *w = 0;
  *h = 0;
  target->getTextBounds(text, 0, 0, x, y, w, h);
}

int clockVisibleX() {
  return constrain(clockSpriteX, 0, max(0, vuWidth - clockW));
}

int analogClockDiameter() {
  return static_cast<int>(min(vuWidth, vuHeight) * 0.875f);
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

bool ensureFftSprite(int width, int height) {
  width = max(1, width);
  height = max(1, height);
  if (g_fftSprite && g_fftSprite->width() == width && g_fftSprite->height() == height) return true;
  if (g_fftSprite) {
    delete g_fftSprite;
    g_fftSprite = nullptr;
  }
  g_fftSprite = createManagedCanvas(width, height, gfx, true, "fftSprite");
  return g_fftSprite != nullptr;
}

float fftBandWeight(int band, int bands) {
  if (bands <= 1) return 1.0f;
  const float pos = static_cast<float>(band) / static_cast<float>(bands - 1);
  if (pos < 0.33f) {
    const float t = pos / 0.33f;
    return fftLowBoost + (fftMidBoost - fftLowBoost) * t;
  }
  if (pos < 0.66f) return fftMidBoost;
  const float t = (pos - 0.66f) / 0.34f;
  return fftMidBoost + (fftHighBoost - fftMidBoost) * t;
}

float fftAggregateBand(const uint8_t* spectrum, int srcStart, int srcEnd) {
  srcStart = constrain(srcStart, 0, FFT_BANDS - 1);
  srcEnd = constrain(srcEnd, srcStart + 1, FFT_BANDS);
  float sum = 0.0f;
  float peak = 0.0f;
  for (int i = srcStart; i < srcEnd; ++i) {
    const float v = spectrum[i] * fftPreGain;
    peak = max(peak, v);
    sum += v;
  }
  const int count = max(1, srcEnd - srcStart);
  if (fftAggregation == 1) return sum / count;
  if (fftAggregation == 2) {
    float sq = 0.0f;
    for (int i = srcStart; i < srcEnd; ++i) {
      const float v = spectrum[i] * fftPreGain;
      sq += v * v;
    }
    return sqrtf(sq / count);
  }
  return peak;
}

float fftShapeLevel(float raw, int band, int bands) {
  float v = raw - fftNoiseFloor;
  const float gate = max(0.0f, fftNoiseGate);
  if (v <= gate) return 0.0f;
  v -= gate;
  v *= fftGain * fftSensitivity * fftBandWeight(band, bands);
  v = constrain(v, 0.0f, 255.0f);
  float norm = v / 255.0f;
  if (fftCurve == 1) {
    norm = log1pf(norm * fftLogStrength) / log1pf(fftLogStrength);
  } else if (fftCurve == 2) {
    norm = powf(norm, max(0.05f, fftGamma));
  }
  return constrain(norm, 0.0f, 1.0f);
}

uint16_t fftGradientColor(float ratio) {
  return blendRgb565(fftColorBottom, fftColorTop, ratio);
}

}  // namespace

void initWidgetClock() {
  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
}

// ============================================================================
// SCREEN BLOCK: CLOCK 1 + CLOCK 2
// Clock1: digital clock, below the Clock2 block.
// Clock2: analog hands clock, inside "if (currentScreen == ScreenId::Clock2)".
// ============================================================================
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
    // ------------------------------------------------------------------------
    // SCREEN BLOCK: CLOCK 2 - analog hands clock
    // Edit analog clock hands, center, lengths, outline, and second hand here.
    // ------------------------------------------------------------------------
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

    auto drawThickLine = [&](float angle, int length, int thickness, uint16_t color) {
      const float dx = cos(angle);
      const float dy = sin(angle);
      const float px = -dy;
      const float py = dx;
      const int half = thickness / 2;
      for (int t = -half; t <= half; ++t) {
        const int x0 = static_cast<int>(roundf(cx + px * t));
        const int y0 = static_cast<int>(roundf(cy + py * t));
        const int x1 = static_cast<int>(roundf(cx + dx * length + px * t));
        const int y1 = static_cast<int>(roundf(cy + dy * length + py * t));
        analogClockSprite->drawLine(x0, y0, x1, y1, color);
      }
    };

    drawThickLine(hourAngle, hourLen, 7, RGB565_BLACK);
    drawThickLine(hourAngle, hourLen, 5, colorClock);
    drawThickLine(minuteAngle, minLen, 5, RGB565_BLACK);
    drawThickLine(minuteAngle, minLen, 3, colorClock);
    drawThickLine(secondAngle, secLen, 3, colorNeedleRed);
    analogClockSprite->fillCircle(cx, cy, 5, colorClock);
    analogClockSprite->fillCircle(cx, cy, 2, colorNeedleRed);
    gfx->draw16bitRGBBitmap(clockX, clockY, (uint16_t*)analogClockSprite->getFramebuffer(), analogClockW, analogClockH);
    redrawClock = false;
    return;
  }

  // --------------------------------------------------------------------------
  // SCREEN BLOCK: CLOCK 1 - digital clock
  // Edit digital clock text, font placement, and blink colon behavior here.
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  // SCREEN BLOCK: CLOCK 1 - digital clock
  // Edit digital clock text, font placement, and blink colon behavior here.
  // --------------------------------------------------------------------------
  const int spriteDrawX = clockVisibleX();
  copyBackgroundToSprite(clockSprite, clockW, clockH, spriteDrawX, clockY);
  clockSprite->setFont(&CLOCK_FONT);

  // Определяем, мигаем ли мы сейчас (двоеточие черное)
  const bool colonOff = BLINK_COLON && ((millis() / 500) % 2 != 0);
  
  // Вычисляем позиции для центрирования
  int16_t x = 0;
  int16_t y = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  measureClockText(clockSprite, clockBuf, &x, &y, &w, &h);
  const int drawX = max(0, (clockW - static_cast<int>(w)) / 2) - x;
  const int drawY = max(0, (clockH - static_cast<int>(h)) / 2) - y;
  
  // Рисуем часы отдельными частями
  clockSprite->setCursor(drawX, drawY);
  
  if (SHOW_SECONDS) {
    // Часы
    clockSprite->setTextColor(colorClock);
    if (timeinfo.tm_hour < 10) clockSprite->print("0");
    clockSprite->print(timeinfo.tm_hour);
    
    // Двоеточие 1
    clockSprite->setTextColor(colonOff ? RGB565_BLACK : colorClock);
    clockSprite->print(":");
    
    // Минуты
    clockSprite->setTextColor(colorClock);
    if (timeinfo.tm_min < 10) clockSprite->print("0");
    clockSprite->print(timeinfo.tm_min);
    
    // Двоеточие 2
    clockSprite->setTextColor(colonOff ? RGB565_BLACK : colorClock);
    clockSprite->print(":");
    
    // Секунды
    clockSprite->setTextColor(colorClock);
    if (timeinfo.tm_sec < 10) clockSprite->print("0");
    clockSprite->print(timeinfo.tm_sec);
  } else {
    // Часы
    clockSprite->setTextColor(colorClock);
    if (timeinfo.tm_hour < 10) clockSprite->print("0");
    clockSprite->print(timeinfo.tm_hour);
    
    // Двоеточие
    clockSprite->setTextColor(colonOff ? RGB565_BLACK : colorClock);
    clockSprite->print(":");
    
    // Минуты
    clockSprite->setTextColor(colorClock);
    if (timeinfo.tm_min < 10) clockSprite->print("0");
    clockSprite->print(timeinfo.tm_min);
  }

  gfx->draw16bitRGBBitmap(spriteDrawX, clockY, (uint16_t*)clockSprite->getFramebuffer(), clockW, clockH);
  redrawClock = false;
}

// // ============================================================================
// // SCREEN BLOCK: VU METER 1 - bars / segmented spectrum
// // Edit bar geometry, gradient, segments, peaks, and left/right bars here.
// // ============================================================================
// void drawVuBars() {
//   if (!gfx || !bgSprite) return;

//   const int canvasW = max(1, min(vuWidth, static_cast<int>(gfx->width())));
//   const int canvasH = max(1, min(vuHeight, static_cast<int>(gfx->height())));
//   const int marginX = max(8, canvasW / 20);
//   const int marginY = max(8, canvasH / 20);
//   const int gapY = max(8, canvasH / 20);
//   const int outerW = max(40, canvasW - marginX * 2);
//   const int outerH = constrain(vuBarsHeight, 6, max(8, (canvasH - marginY * 2 - gapY) / 2));
//   const int centeredX = (canvasW - outerW) / 2;
//   const int centeredY = (canvasH - (outerH * 2 + gapY)) / 2;
//   const int drawX = constrain(centeredX + vuBarsOffsetX, 0, max(0, canvasW - outerW));
//   const int topY = constrain(centeredY + vuBarsOffsetY, 0, max(0, canvasH - (outerH * 2 + gapY)));
//   const int blockY = topY;
//   const int blockH = outerH * 2 + gapY;

//   const int innerPad = 2;
//   const int innerX = drawX + innerPad;
//   const int innerW = max(1, outerW - innerPad * 2);
//   const int innerH = max(1, outerH - innerPad * 2);
//   const int segments = constrain(vuBarsSegments, 4, 40);
//   const int segGap = max(2, innerW / 120);
//   const int segW = max(1, (innerW - segGap * (segments - 1)) / segments);

//   ensureVuBarsSprite(canvasW, canvasH);
//   if (!g_vuBarsSprite) return;

//   uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
//   uint16_t* spriteBuf = (uint16_t*)g_vuBarsSprite->getFramebuffer();
//   if (!bgBuf || !spriteBuf) return;

//   for (int y = 0; y < blockH; ++y) {
//     const int srcY = blockY + y;
//     if (srcY < 0 || srcY >= canvasH) {
//       memset(&spriteBuf[y * canvasW], 0, canvasW * sizeof(uint16_t));
//     } else {
//       memcpy(&spriteBuf[y * canvasW], &bgBuf[srcY * vuWidth], canvasW * sizeof(uint16_t));
//     }
//   }

//   auto barColor = [](float ratio) -> uint16_t {
//     return blendRgb565(vuBarsColorBottom, vuBarsColorTop, ratio);
//   };

//   static int vu1VisibleL = 0;
//   static int vu1VisibleR = 0;
//   static int vu1PeakL = 0;
//   static int vu1PeakR = 0;
//   static int vu1LastInnerW = 0;
//   static uint8_t vu1PeakFallCounter = 0;

//   if (vu1LastInnerW != innerW) {
//     vu1VisibleL = 0;
//     vu1VisibleR = 0;
//     vu1PeakL = 0;
//     vu1PeakR = 0;
//     vu1LastInnerW = innerW;
//   }

//   int targetVisibleL = 0;
//   int targetVisibleR = 0;
//   if (radioStarted && !isPaused) {
//     const int vu = audio.getVUlevel();
//     targetVisibleL = map((vu >> 8) & 0xFF, 0, 255, 0, innerW);
//     targetVisibleR = map(vu & 0xFF, 0, 255, 0, innerW);
//   }
//   targetVisibleL = constrain(targetVisibleL, 0, innerW);
//   targetVisibleR = constrain(targetVisibleR, 0, innerW);

//   const int fallStep = max(1, innerW / 80);
//   auto updateVisible = [&](int target, int& visible) {
//     if (target >= visible) {
//       visible = target;
//     } else {
//       visible = max(target, visible - fallStep);
//     }
//   };
//   updateVisible(targetVisibleL, vu1VisibleL);
//   updateVisible(targetVisibleR, vu1VisibleR);

//   vu1PeakL = max(vu1PeakL, vu1VisibleL);
//   vu1PeakR = max(vu1PeakR, vu1VisibleR);
//   vu1PeakFallCounter++;
//   if (vu1PeakFallCounter >= 3) {
//     vu1PeakFallCounter = 0;
//     const int peakFallStep = max(1, innerW / 120);
//     vu1PeakL = max(vu1VisibleL, vu1PeakL - peakFallStep);
//     vu1PeakR = max(vu1VisibleR, vu1PeakR - peakFallStep);
//   }

//   auto segmentColor = [&](int index) -> uint16_t {
//     const float ratio = segments > 1 ? static_cast<float>(index) / static_cast<float>(segments - 1) : 0.0f;
//     return barColor(ratio);
//   };

//   auto drawSegmentedBar = [&](int visiblePixels, int peakPixels, int yInBlock) {
//     for (int i = 0; i < segments; ++i) {
//       const int sx = innerX + i * (segW + segGap);
//       const int sy = yInBlock + innerPad;
//       const int segmentCenter = sx + segW / 2;
//       if (segmentCenter - innerX <= visiblePixels) {
//         g_vuBarsSprite->fillRect(sx, sy, segW, innerH, segmentColor(i));
//       } else {
//         g_vuBarsSprite->drawRect(sx, sy, segW, innerH, RGB565_GRAY);
//       }
//     }

//     if (peakPixels > 0) {
//       const int peakWidth = constrain(max(3, segW), 3, max(3, innerW));
//       const int px = constrain(innerX + peakPixels - peakWidth / 2, innerX, innerX + innerW - peakWidth);
//       const int py = yInBlock + innerPad;
//       g_vuBarsSprite->drawRect(px, py, peakWidth, innerH, vuBarsColorBottom);
//       if (peakWidth > 2 && innerH > 2) {
//         g_vuBarsSprite->fillRect(px + 1, py + 1, peakWidth - 2, innerH - 2, vuBarsColorTop);
//       }
//     }
//   };

//   drawSegmentedBar(vu1VisibleL, vu1PeakL, 0);
//   drawSegmentedBar(vu1VisibleR, vu1PeakR, outerH + gapY);

//   gfx->draw16bitRGBBitmap(0, blockY, (uint16_t*)g_vuBarsSprite->getFramebuffer(), canvasW, blockH);
// }

// ============================================================================
// SCREEN BLOCK: FFT - spectrum analyzer window
// Edit FFT window size, margins, bands, colors, peak hold, and smoothing here.
// ============================================================================
void drawFftWindow() {
  if (!gfx || !bgSprite) return;

  uint8_t spectrum[FFT_BANDS] = {0};
  const uint8_t got = (radioStarted && !isPaused) ? audio.getSpectrum(spectrum, FFT_BANDS) : 0;
  const bool spectrumActive = got > 0;
  if (got == 0) memset(spectrum, 0, sizeof(spectrum));

  const int screenW = max(1, min(vuWidth, static_cast<int>(gfx->width())));
  const int screenH = max(1, min(vuHeight, static_cast<int>(gfx->height())));
  const int drawW = constrain(fftWindowW, 40, screenW);
  const int drawH = constrain(fftWindowH, 40, screenH);
  const int drawX = constrain(fftWindowX, 0, max(0, screenW - drawW));
  const int drawY = constrain(fftWindowY, 0, max(0, screenH - drawH));
  if (!ensureFftSprite(drawW, drawH) || !g_fftSprite) return;

  copyBackgroundToSprite(g_fftSprite, drawW, drawH, drawX, drawY);

  const int bands = constrain(fftBands, 1, FFT_BANDS);
  const int minBin = constrain(fftMinBin, 0, FFT_BANDS - 1);
  const int maxBin = constrain(fftMaxBin, minBin, FFT_BANDS - 1);
  const int binSpan = max(1, maxBin - minBin + 1);
  const int side = constrain(fftSideMargin, 0, min(80, drawW / 2));
  const int top = constrain(fftTopMargin, 0, min(100, drawH - 1));
  const int bottom = constrain(fftBottomMargin, 0, min(100, drawH - 1));
  const int graphW = max(1, drawW - side * 2);
  const int graphH = max(1, drawH - top - bottom);
  const int gap = constrain(fftBarGap, 0, 12);
  const int barW = max(1, (graphW - gap * (bands - 1)) / bands);
  const int usedW = barW * bands + gap * (bands - 1);
  const int startX = side + max(0, (graphW - usedW) / 2);
  const int segments = constrain(fftSegments, 1, 80);
  const int segGap = max(1, graphH / 160);
  const int segH = max(1, (graphH - segGap * (segments - 1)) / segments);
  const int usedH = segH * segments + segGap * (segments - 1);
  const int baseY = top + max(0, (graphH - usedH) / 2) + usedH;
  for (int b = 0; b < bands; ++b) {
    const int visibleIndex = fftMirror ? (bands - 1 - b) : b;
    const int srcStart = minBin + (visibleIndex * binSpan) / bands;
    const int srcEnd = minBin + ((visibleIndex + 1) * binSpan) / bands;
    const float raw = fftAggregateBand(spectrum, srcStart, max(srcStart + 1, srcEnd));
    const float target = spectrumActive ? fftShapeLevel(raw, visibleIndex, bands) : 0.0f;
    const bool bandActive = target > 0.0f || g_fftSmoothed[b] > 0.01f;
    const float coeff = target > g_fftSmoothed[b] ? fftAttack : fftRelease;
    g_fftSmoothed[b] += (target - g_fftSmoothed[b]) * constrain(coeff, 0.0f, 1.0f);
    if (target <= 0.0f && g_fftSmoothed[b] < 0.01f) g_fftSmoothed[b] = 0.0f;
    g_fftSmoothed[b] = constrain(g_fftSmoothed[b], 0.0f, 1.0f);

    int active = static_cast<int>(g_fftSmoothed[b] * segments + 0.5f);
    if (active > 0) active = max(active, fftMinLitSegments);
    active = constrain(active, 0, segments);
    if (fftInvert) active = segments - active;

    if (fftPeakHold) {
      g_fftPeaks[b] = max(g_fftPeaks[b] - (fftPeakFall / 255.0f), g_fftSmoothed[b]);
      g_fftPeaks[b] = constrain(g_fftPeaks[b], 0.0f, 1.0f);
    } else {
      g_fftPeaks[b] = 0.0f;
    }

    const int x = startX + b * (barW + gap);
    for (int s = 0; s < segments; ++s) {
      const int y = baseY - segH - s * (segH + segGap);
      const bool on = s < active;
      const float ratio = segments > 1 ? static_cast<float>(s) / static_cast<float>(segments - 1) : 0.0f;
      if (on) {
        g_fftSprite->fillRect(x, y, barW, segH, fftGradientColor(ratio));
      } else {
        g_fftSprite->fillRect(x, y, barW, segH, fftColorOff);
      }
    }

    if (fftPeakHold && segments > 0) {
      const int peakSeg = constrain(static_cast<int>(g_fftPeaks[b] * segments + 0.5f), 0, segments - 1);
      const int y = baseY - segH - peakSeg * (segH + segGap);
      g_fftSprite->fillRect(x, y, barW, max(1, segH / 2), fftColorPeak);
    }
  }

  for (int b = bands; b < FFT_BANDS; ++b) {
    g_fftSmoothed[b] = 0.0f;
    g_fftPeaks[b] = 0.0f;
  }

  gfx->draw16bitRGBBitmap(drawX, drawY, (uint16_t*)g_fftSprite->getFramebuffer(), drawW, drawH);
}

// ============================================================================
// SCREEN BLOCK: VU METER 3 - cardio / waveform window
// Edit waveform window, pulse shape, trace colors, and mirror trace here.
// ============================================================================
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

// ============================================================================
// SCREEN BLOCK: VU METER 2 - needle level calculation
// Edit audio-to-needle response, attack/release, and smoothing here.
// The actual needle drawing is in drawVuNeedleChannel() below.
// ============================================================================
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

// ============================================================================
// SPRITE BLOCK: shared screen sprites
// Edit common canvas sizes and sprite allocation for backgrounds/clocks/needles.
// ============================================================================
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
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  measureClockText(gfx, clockMaxText(), &x1, &y1, &w, &h);
  const int clockPad = 6;
  clockW = min(vuWidth, static_cast<int>(w) + clockPad * 2);
  clockH = min(vuHeight, static_cast<int>(h) + clockPad * 2);
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

  copyBackgroundToSprite(clockSprite, clockW, clockH, clockVisibleX(), clockSpriteY);

  uint16_t* analogBuf = (uint16_t*)analogClockSprite->getFramebuffer();
  for (int y = 0; y < analogClockH; y++) {
    memset(&analogBuf[y * analogClockW], 0, analogClockW * sizeof(uint16_t));
  }
}

// ============================================================================
// SCREEN BLOCK: VU METER 2 - needle drawing
// Edit needle position, pivot, angle, thickness, colors, rotation, and borders.
// Called twice: left channel and right channel.
// ============================================================================
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
