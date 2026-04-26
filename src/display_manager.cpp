#include "display_manager.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Audio.h>
#include <cmath>
#include <vector>

#include "config.h"
#include "display_text_utils.h"
#include "psram_canvas.h"
#include "screens/screen_manager.h"
#include "screens/screens.h"
#include "sleep_timer.h"

extern Arduino_GFX* gfx;
extern Arduino_Canvas* bgSprite;
extern Arduino_Canvas* needleSpriteL;
extern Arduino_Canvas* needleSpriteR;
extern Arduino_Canvas* clockSprite;
extern Arduino_Canvas* analogClockSprite;
extern Audio audio;
extern SemaphoreHandle_t xMutex;
extern SleepTimer sleepTimer;

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
extern int clockSpriteX;
extern int clockSpriteY;
extern int currentVolume;
extern int currentStreamIndex;
extern int streamCount;
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
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern bool vuMeterEnabled;
extern bool primaryMarqueeEnabled;
extern bool secondaryMarqueeEnabled;
extern bool quoteMarqueeEnabled;
extern int needleRotation;
extern int needleSpriteWidth;
extern int needleSpriteHeight;
extern int needleMinAngle;
extern int needleMaxAngle;
extern int stationNameX;
extern int stationNameY;
extern int streamTitle1X;
extern int streamTitle1Y;
extern int streamTitle2X;
extern int streamTitle2Y;
extern int stationScrollWidth;
extern int title1ScrollWidth;
extern int title2ScrollWidth;
extern int marqueePauseMs;
extern int bitrateX;
extern int bitrateY;
extern int ipX;
extern int ipY;
extern int quoteX;
extern int quoteY;
extern int quoteW;
extern int clockX;
extern int clockY;
extern int volumeX;
extern int volumeY;
extern uint16_t colorStation;
extern uint16_t colorTitle;
extern uint16_t colorBitrate;
extern uint16_t colorVolume;
extern uint16_t colorClock;
extern uint16_t colorNeedleMain;
extern uint16_t colorNeedleRed;
extern uint16_t colorIP;

extern bool wifiConnected;
extern bool radioStarted;
extern bool isPaused;
extern bool metadataPending;
extern bool redrawClock;
extern bool showStationNumber;
extern unsigned long metadataRequestTime;
extern String stationNumberFormat;
extern int stationTextFontSize;
extern String prevStreamName;
extern String currentStreamName;
extern String icy_station;
extern String icy_streamtitle;
extern String icy_bitrate;
extern String audio_codec;
extern String audio_frequency;
extern String audio_bitness;
extern String currentBackground;
extern String quoteApiUrl;
extern void selectStream(int idx);

void startRadio() {
  if (!wifiConnected || streamCount == 0) return;

  if (radioStarted && isPaused) {
    audio.pauseResume();
    isPaused = false;
    //Serial.println("Radio resumed");
    redrawScreen();
    return;
  }

  if (!radioStarted) {
    icy_station = "";
    icy_streamtitle = "";
    icy_bitrate = "";
    metadataPending = true;
    metadataRequestTime = millis();
    selectStream(currentStreamIndex);
    audio.setVolume(currentVolume);
    radioStarted = true;
    isPaused = false;
    //Serial.println("Radio started");
    redrawScreen();
  }
}

static String trimTextToWidth(Arduino_GFX* display, String text, int maxWidth) {
  int16_t x1, y1;
  uint16_t w, h;
  String encoded = encodeDisplayText(text);
  String original = encoded;

  while (encoded.length() > 0) {
    display->getTextBounds(encoded.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth) break;
    encoded.remove(encoded.length() - 1);
  }

  if (encoded.length() < original.length()) {
    String withDots = encoded + "...";
    display->getTextBounds(withDots.c_str(), 0, 0, &x1, &y1, &w, &h);
    while (w > maxWidth && encoded.length() > 0) {
      encoded.remove(encoded.length() - 1);
      withDots = encoded + "...";
      display->getTextBounds(withDots.c_str(), 0, 0, &x1, &y1, &w, &h);
    }
    encoded = withDots;
  }

  return encoded;
}

namespace {
struct MarqueeLineState {
  String text;
  int offset = 0;
  int cycleWidth = 0;
  int pauseTicks = 0;
};

Arduino_Canvas* g_metadataLineSprites[3] = {nullptr, nullptr, nullptr};
Arduino_Canvas* g_sceneSprite = nullptr;
Arduino_Canvas* g_vuBarsSprite = nullptr;
Arduino_Canvas* g_vuCardioSprite = nullptr;
Arduino_Canvas* g_quoteLineSprite = nullptr;
MarqueeLineState g_marqueeLines[3];
MarqueeLineState g_quoteMarquee;
unsigned long g_lastMetadataFrameMs = 0;
float g_vuPeakL = 0.0f;
float g_vuPeakR = 0.0f;
unsigned long g_vuPeakLastMs = 0;
float g_vuNeedleLevelL = 0.0f;
float g_vuNeedleLevelR = 0.0f;
unsigned long g_vuNeedleLastMs = 0;
float g_vuCardioLevel = 0.0f;
float g_vuCardioFloor = 0.0f;
float g_vuCardioCeiling = 0.32f;
float g_vuCardioTransient = 0.0f;
unsigned long g_vuCardioLastMs = 0;
float g_vuCardioAutoGain = 1.0f;
float g_vuCardioPeak = 0.0f;
float g_vuCardioRms = 0.0f;
std::vector<int16_t> g_vuCardioWaveform;
volatile bool g_displayTaskPaused = false;
constexpr unsigned long kMetadataFrameIntervalMs = 30;
const char* kMarqueeSeparator = "   |   ";

struct FontLineMetrics {
  int ascent = 0;
  int descent = 0;
  int baseline = 0;
  int height = 0;
};

FontLineMetrics getFontLineMetrics(const GFXfont* font) {
  FontLineMetrics metrics;
  if (!font) {
    metrics.ascent = 8;
    metrics.descent = 2;
    metrics.baseline = 10;
    metrics.height = 12;
    return metrics;
  }

  int minYOffset = 0;
  int maxBottom = 0;
  for (uint16_t c = font->first; c <= font->last; ++c) {
    const GFXglyph* glyph = font->glyph + (c - font->first);
    if (!glyph->width || !glyph->height) continue;
    minYOffset = min(minYOffset, int(glyph->yOffset));
    maxBottom = max(maxBottom, int(glyph->yOffset) + int(glyph->height));
  }

  const int topPadding = 2;
  const int bottomPadding = 2;
  metrics.ascent = -minYOffset;
  metrics.descent = max(0, maxBottom);
  metrics.baseline = metrics.ascent + topPadding;
  metrics.height = max(1, metrics.ascent + metrics.descent + topPadding + bottomPadding);
  return metrics;
}

int marqueePauseTicks() {
  return max(0, (marqueePauseMs + int(kMetadataFrameIntervalMs) - 1) / int(kMetadataFrameIntervalMs));
}

int metadataClipWidthForX(int x, int requestedWidth) {
  if (!gfx) return 0;
  const int requested = constrain(requestedWidth > 0 ? requestedWidth : gfx->width(), 40, gfx->width());
  return max(0, min(requested, gfx->width() - x));
}

int quoteClipWidth() {
  if (!gfx) return 0;
  const int startX = constrain(quoteX, 0, gfx->width() - 1);
  const int maxAvail = gfx->width() - startX;
  if (maxAvail <= 0) return 0;
  const int requested = quoteW > 0 ? quoteW : maxAvail;
  return constrain(requested, 40, maxAvail);
}

const GFXfont* getStationTextFont() {
  switch (constrain(stationTextFontSize, 0, 2)) {
    case 0:
      return &STATION_FONT_SMALL;
    case 2:
      return &STATION_FONT_LARGE;
    default:
      return &STATION_FONT_NORMAL;
  }
}

void ensureMetadataLineSprite(int index, int width, int height) {
  if (!gfx || index < 0 || index >= 3 || width <= 0) return;
  if (g_metadataLineSprites[index] != nullptr &&
      g_metadataLineSprites[index]->width() == width &&
      g_metadataLineSprites[index]->height() == height) {
    return;
  }
  if (g_metadataLineSprites[index]) {
    delete g_metadataLineSprites[index];
    g_metadataLineSprites[index] = nullptr;
  }
  String spriteName = "metadataLine" + String(index);
  g_metadataLineSprites[index] = createManagedCanvas(width, height, gfx, true, spriteName.c_str());
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

void ensureQuoteLineSprite(int width, int height) {
  if (!gfx || width <= 0) return;
  if (g_quoteLineSprite != nullptr &&
      g_quoteLineSprite->width() == width &&
      g_quoteLineSprite->height() == height) {
    return;
  }
  if (g_quoteLineSprite) {
    delete g_quoteLineSprite;
    g_quoteLineSprite = nullptr;
  }
  g_quoteLineSprite = createManagedCanvas(width, height, gfx, true, "quoteLine");
}

void buildMetadataLines(String (&lines)[3]) {
  String stationLine = currentStreamName;
  stationLine.trim();

  if (showStationNumber && currentStreamIndex >= 0 && streamCount > 0) {
    String numberText = stationNumberFormat.length() ? stationNumberFormat : "{cur}";
    numberText.replace("{cur}", String(currentStreamIndex + 1));
    numberText.replace("{total}", String(streamCount));
    numberText.trim();

    if (numberText.length()) {
      stationLine = stationLine.length() ? numberText + ". " + stationLine : numberText;
    }
  }

  lines[0] = stationLine.length() ? stationLine : "---";

  lines[1] = "---";
  lines[2] = icy_streamtitle.length() > 0 ? icy_streamtitle : "---";

  const int dashPos = icy_streamtitle.indexOf(" - ");
  if (dashPos >= 0) {
    lines[1] = icy_streamtitle.substring(0, dashPos);
    lines[2] = icy_streamtitle.substring(dashPos + 3);
    lines[1].trim();
    lines[2].trim();
    if (!lines[1].length()) lines[1] = "---";
    if (!lines[2].length()) lines[2] = "---";
  } else if (icy_streamtitle.length() > 0) {
    lines[1] = icy_streamtitle;
    lines[2] = "---";
  }
}

bool isMetadataLineEnabled(int index) {
  return index == 0 ? primaryMarqueeEnabled : secondaryMarqueeEnabled;
}

int measureTextAdvance(const String& text, const GFXfont* font) {
  if (!font) {
    return text.length() * 6;
  }

  int width = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    uint8_t c = static_cast<uint8_t>(text[i]);
    if (c < font->first || c > font->last) continue;
    const GFXglyph* glyph = font->glyph + (c - font->first);
    width += glyph->xAdvance;
  }
  return width;
}

void copyMetadataBackgroundToSprite(Arduino_Canvas* sprite, int drawX, int drawY) {
  if (!sprite || !bgSprite) return;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* spriteBuf = (uint16_t*)sprite->getFramebuffer();
  if (!bgBuf || !spriteBuf) return;

  const int spriteW = sprite->width();
  const int spriteH = sprite->height();
  for (int y = 0; y < spriteH; ++y) {
    const int srcY = drawY + y;
    if (srcY < 0 || srcY >= vuHeight) {
      memset(&spriteBuf[y * spriteW], 0, spriteW * sizeof(uint16_t));
      continue;
    }

    if (drawX >= 0 && drawX + spriteW <= vuWidth) {
      memcpy(&spriteBuf[y * spriteW], &bgBuf[srcY * vuWidth + drawX], spriteW * sizeof(uint16_t));
      continue;
    }

    for (int x = 0; x < spriteW; ++x) {
      const int srcX = drawX + x;
      spriteBuf[y * spriteW + x] = (srcX >= 0 && srcX < vuWidth) ? bgBuf[srcY * vuWidth + srcX] : RGB565_BLACK;
    }
  }
}

void drawMarqueeLine(int index, const String& text, MarqueeLineState& state, int x, int baselineY, uint16_t color, const GFXfont* font) {
  if (!gfx || index < 0 || index >= 3) return;
  const int requestedWidths[3] = {stationScrollWidth, title1ScrollWidth, title2ScrollWidth};
  const int clipWidth = metadataClipWidthForX(x, requestedWidths[index]);
  if (clipWidth <= 0) return;

  const FontLineMetrics metrics = getFontLineMetrics(font);
  ensureMetadataLineSprite(index, clipWidth, metrics.height);
  Arduino_Canvas* lineSprite = g_metadataLineSprites[index];
  if (!lineSprite) return;

  String repeatedText = text + String(kMarqueeSeparator);
  const int repeatedAdvance = measureTextAdvance(repeatedText, font);
  const int topY = max(0, baselineY - metrics.baseline);
  const int availableH = min(int(lineSprite->height()), gfx->height() - topY);
  if (availableH <= 0) return;

  copyMetadataBackgroundToSprite(lineSprite, x, topY);
  lineSprite->setFont(font);
  lineSprite->setTextColor(color);
  lineSprite->setTextWrap(false);

  const int baselineInSprite = baselineY - topY;
  if (repeatedAdvance > clipWidth) {
    const int drawX = -state.offset;
    lineSprite->setCursor(drawX, baselineInSprite);
    lineSprite->print(repeatedText);
    lineSprite->setCursor(drawX + state.cycleWidth, baselineInSprite);
    lineSprite->print(repeatedText);
  } else {
    lineSprite->setCursor(0, baselineInSprite);
    lineSprite->print(text);
  }

  gfx->draw16bitRGBBitmap(x, topY, (uint16_t*)lineSprite->getFramebuffer(), clipWidth, availableH);
}

void drawMetadataMarquees(bool advance) {
  if (!gfx) return;

  const GFXfont* font = getStationTextFont();
  String lines[3];
  buildMetadataLines(lines);

  const int lineXs[3] = {stationNameX, streamTitle1X, streamTitle2X};
  const int lineYs[3] = {stationNameY, streamTitle1Y, streamTitle2Y};
  const uint16_t lineColors[3] = {colorStation, colorTitle, colorTitle};

  gfx->setFont(font);
  for (int i = 0; i < 3; ++i) {
    if (!isMetadataLineEnabled(i)) continue;
    const int requestedWidths[3] = {stationScrollWidth, title1ScrollWidth, title2ScrollWidth};
    const int clipWidth = metadataClipWidthForX(lineXs[i], requestedWidths[i]);
    if (clipWidth <= 0) continue;

    String repeatedText = lines[i] + String(kMarqueeSeparator);
    lines[i] = encodeDisplayText(lines[i]);
    repeatedText = lines[i] + String(kMarqueeSeparator);
    const int cycle = measureTextAdvance(repeatedText, font);

    if (g_marqueeLines[i].text != lines[i]) {
      g_marqueeLines[i].text = lines[i];
      g_marqueeLines[i].offset = 0;
      g_marqueeLines[i].cycleWidth = cycle;
      g_marqueeLines[i].pauseTicks = marqueePauseTicks();
    } else {
      g_marqueeLines[i].cycleWidth = cycle;
    }

    if (cycle <= clipWidth) {
      g_marqueeLines[i].offset = 0;
      g_marqueeLines[i].pauseTicks = 0;
    } else if (advance && g_marqueeLines[i].cycleWidth > 0) {
      if (g_marqueeLines[i].pauseTicks > 0) {
        g_marqueeLines[i].pauseTicks--;
      } else {
        g_marqueeLines[i].offset++;
        if (g_marqueeLines[i].offset >= g_marqueeLines[i].cycleWidth) {
          g_marqueeLines[i].offset = 0;
          g_marqueeLines[i].pauseTicks = marqueePauseTicks();
        }
      }
    }

    drawMarqueeLine(i, lines[i], g_marqueeLines[i], lineXs[i], lineYs[i], lineColors[i], font);
  }
}

void drawPrimaryMarquee(bool advance) {
  if (!gfx) return;

  const GFXfont* font = getStationTextFont();
  String lines[3];
  buildMetadataLines(lines);

  const int clipWidth = metadataClipWidthForX(stationNameX, stationScrollWidth);
  if (clipWidth <= 0) return;

  const int i = 0;
  lines[i] = encodeDisplayText(lines[i]);
  String repeatedText = lines[i] + String(kMarqueeSeparator);
  const int cycle = measureTextAdvance(repeatedText, font);

  if (g_marqueeLines[i].text != lines[i]) {
    g_marqueeLines[i].text = lines[i];
    g_marqueeLines[i].offset = 0;
    g_marqueeLines[i].cycleWidth = cycle;
    g_marqueeLines[i].pauseTicks = marqueePauseTicks();
  } else {
    g_marqueeLines[i].cycleWidth = cycle;
  }

  if (cycle <= clipWidth) {
    g_marqueeLines[i].offset = 0;
    g_marqueeLines[i].pauseTicks = 0;
  } else if (advance && g_marqueeLines[i].cycleWidth > 0) {
    if (g_marqueeLines[i].pauseTicks > 0) {
      g_marqueeLines[i].pauseTicks--;
    } else {
      g_marqueeLines[i].offset++;
      if (g_marqueeLines[i].offset >= g_marqueeLines[i].cycleWidth) {
        g_marqueeLines[i].offset = 0;
        g_marqueeLines[i].pauseTicks = marqueePauseTicks();
      }
    }
  }

  drawMarqueeLine(i, lines[i], g_marqueeLines[i], stationNameX, stationNameY, colorStation, font);
}

void drawSecondaryMarquees(bool advance) {
  if (!gfx || !secondaryMarqueeEnabled) return;

  const GFXfont* font = getStationTextFont();
  String lines[3];
  buildMetadataLines(lines);

  const int lineXs[3] = {stationNameX, streamTitle1X, streamTitle2X};
  const int lineYs[3] = {stationNameY, streamTitle1Y, streamTitle2Y};
  const uint16_t lineColors[3] = {colorStation, colorTitle, colorTitle};

  gfx->setFont(font);
  for (int i = 1; i < 3; ++i) {
    const int requestedWidths[3] = {stationScrollWidth, title1ScrollWidth, title2ScrollWidth};
    const int clipWidth = metadataClipWidthForX(lineXs[i], requestedWidths[i]);
    if (clipWidth <= 0) continue;

    lines[i] = encodeDisplayText(lines[i]);
    String repeatedText = lines[i] + String(kMarqueeSeparator);
    const int cycle = measureTextAdvance(repeatedText, font);

    if (g_marqueeLines[i].text != lines[i]) {
      g_marqueeLines[i].text = lines[i];
      g_marqueeLines[i].offset = 0;
      g_marqueeLines[i].cycleWidth = cycle;
      g_marqueeLines[i].pauseTicks = marqueePauseTicks();
    } else {
      g_marqueeLines[i].cycleWidth = cycle;
    }

    if (cycle <= clipWidth) {
      g_marqueeLines[i].offset = 0;
      g_marqueeLines[i].pauseTicks = 0;
    } else if (advance && g_marqueeLines[i].cycleWidth > 0) {
      if (g_marqueeLines[i].pauseTicks > 0) {
        g_marqueeLines[i].pauseTicks--;
      } else {
        g_marqueeLines[i].offset++;
        if (g_marqueeLines[i].offset >= g_marqueeLines[i].cycleWidth) {
          g_marqueeLines[i].offset = 0;
          g_marqueeLines[i].pauseTicks = marqueePauseTicks();
        }
      }
    }

    drawMarqueeLine(i, lines[i], g_marqueeLines[i], lineXs[i], lineYs[i], lineColors[i], font);
  }
}

void resetMetadataMarquees() {
  for (int i = 0; i < 3; ++i) {
    g_marqueeLines[i].text = "";
    g_marqueeLines[i].offset = 0;
    g_marqueeLines[i].cycleWidth = 0;
    g_marqueeLines[i].pauseTicks = 0;
    if (g_metadataLineSprites[i]) {
      delete g_metadataLineSprites[i];
      g_metadataLineSprites[i] = nullptr;
    }
  }
  g_quoteMarquee.text = "";
  g_quoteMarquee.offset = 0;
  g_quoteMarquee.cycleWidth = 0;
  g_quoteMarquee.pauseTicks = 0;
  if (g_quoteLineSprite) {
    delete g_quoteLineSprite;
    g_quoteLineSprite = nullptr;
  }
}
}

static void drawDataLines() {
  gfx->setFont(&STATION_FONT_NORMAL);
  const int maxTextWidth = 300;
  drawMetadataMarquees(false);

  gfx->setFont(&BITRATE_FONT);
  gfx->setTextColor(colorBitrate);
  gfx->setCursor(bitrateX, bitrateY);

  String audioInfoLine1 = "";
  if (audio_codec.length() > 0 && !audio_codec.equalsIgnoreCase("unknown")) {
    audioInfoLine1 = audio_codec;
  }
  int br = icy_bitrate.toInt();
  if (br <= 0) br = audio.getBitRate() / 1000;
  if (br > 0) {
    if (audioInfoLine1.length() > 0) audioInfoLine1 += ":";
    audioInfoLine1 += String(br) + "k";
  }
  if (audioInfoLine1.length() == 0) audioInfoLine1 = "---";
  audioInfoLine1 = trimTextToWidth(gfx, audioInfoLine1, maxTextWidth);
  gfx->print(audioInfoLine1);

  int16_t x1, y1;
  uint16_t w1, h1;
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w1, &h1);
  int secondLineY = bitrateY + h1 + 5;
  gfx->setCursor(bitrateX, secondLineY);

  String audioInfoLine2 = "";
  if (audio_frequency.length() > 0) audioInfoLine2 = audio_frequency;
  if (audio_bitness.length() > 0) {
    if (audioInfoLine2.length() > 0) audioInfoLine2 += ":";
    audioInfoLine2 += audio_bitness;
  }
  if (audioInfoLine2.length() == 0) audioInfoLine2 = "---";
  audioInfoLine2 = trimTextToWidth(gfx, audioInfoLine2, maxTextWidth);
  gfx->print(audioInfoLine2);

  int thirdLineY = ipY;
  gfx->setCursor(ipX, thirdLineY);
  gfx->setTextColor(colorIP);
  String ipInfo = "";
  if (wifiConnected) ipInfo = WiFi.localIP().toString();
  else if (WiFi.getMode() == WIFI_AP) ipInfo = "AP: " + WiFi.softAPIP().toString();
  else ipInfo = "No IP";
  ipInfo = trimTextToWidth(gfx, ipInfo, maxTextWidth);
  gfx->print(ipInfo);
}

namespace {
String g_clockQuote = "";
unsigned long g_clockQuoteLastFetchMs = 0;
bool g_clockQuoteFetchInProgress = false;
constexpr unsigned long kClockQuoteRefreshMs = 10UL * 60UL * 1000UL; //время смены цитаты

String decodeQuoteText(String text) {
  text.replace("\\n", " ");
  text.replace("\\r", " ");
  text.replace("\\t", " ");
  text.replace("\\\"", "\"");
  text.replace("\\'", "'");
  text.replace("&quot;", "\"");
  text.trim();
  while (text.indexOf("  ") >= 0) {
    text.replace("  ", " ");
  }
  return text;
}

String extractJsonField(const String& body, const char* fieldName) {
  const String pattern = String("\"") + fieldName + "\":\"";
  const int start = body.indexOf(pattern);
  if (start < 0) return "";

  String out = "";
  for (int i = start + pattern.length(); i < body.length(); ++i) {
    const char c = body[i];
    if (c == '"' && body[i - 1] != '\\') break;
    out += c;
  }
  return decodeQuoteText(out);
}

void updateClockQuoteIfNeeded() {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED || g_clockQuoteFetchInProgress) return;

  const unsigned long now = millis();
  const bool needsInitialFetch = g_clockQuote.isEmpty();
  const bool refreshExpired = (now - g_clockQuoteLastFetchMs) >= kClockQuoteRefreshMs;
  if (!needsInitialFetch && !refreshExpired) return;

  g_clockQuoteFetchInProgress = true;

  HTTPClient http;
  http.setTimeout(6000);
  const char* quoteUrl = quoteApiUrl.length() > 0 ? quoteApiUrl.c_str()
                                                   : "http://api.forismatic.com/api/1.0/?method=getQuote&format=json&lang=ru";
  if (http.begin(quoteUrl)) {
    const int code = http.GET();
    if (code > 0) {
      const String body = http.getString();
      String quote = extractJsonField(body, "quoteText");
      String author = extractJsonField(body, "quoteAuthor");
      if (!quote.isEmpty()) {
        if (!author.isEmpty()) {
          quote += " - ";
          quote += author;
        }
        g_clockQuote = quote;
        g_clockQuoteLastFetchMs = now;
        //Serial.println("Clock quote updated");
      }
    }
    http.end();
  }

  g_clockQuoteFetchInProgress = false;
}

void drawQuoteMarquee(bool advance) {
  if (g_clockQuote.isEmpty()) return;
  if (!gfx || !bgSprite) return;

  const int drawX = constrain(quoteX, 0, gfx->width() - 1);
  const int clipWidth = quoteClipWidth();
  if (clipWidth <= 0) return;

  const GFXfont* font = &QUOTE_FONT;
  const String encodedQuote = encodeDisplayText(g_clockQuote);
  const String repeatedText = encodedQuote + String(kMarqueeSeparator);
  const int cycle = measureTextAdvance(repeatedText, font);

  if (g_quoteMarquee.text != encodedQuote) {
    g_quoteMarquee.text = encodedQuote;
    g_quoteMarquee.offset = 0;
    g_quoteMarquee.cycleWidth = cycle;
    g_quoteMarquee.pauseTicks = marqueePauseTicks();
  } else {
    g_quoteMarquee.cycleWidth = cycle;
  }

  if (cycle <= clipWidth) {
    g_quoteMarquee.offset = 0;
    g_quoteMarquee.pauseTicks = 0;
  } else if (advance && g_quoteMarquee.cycleWidth > 0) {
    if (g_quoteMarquee.pauseTicks > 0) {
      g_quoteMarquee.pauseTicks--;
    } else {
      g_quoteMarquee.offset+=3; //скорость цитат
      if (g_quoteMarquee.offset >= g_quoteMarquee.cycleWidth) {
        g_quoteMarquee.offset = 0;
        g_quoteMarquee.pauseTicks = marqueePauseTicks();
      }
    }
  }

  const int baselineY = constrain(quoteY, 0, gfx->height() - 1);
  const FontLineMetrics metrics = getFontLineMetrics(font);
  const int topY = max(0, baselineY - metrics.baseline);

  ensureQuoteLineSprite(clipWidth, metrics.height);
  if (!g_quoteLineSprite) return;

  const int availableH = min(int(g_quoteLineSprite->height()), gfx->height() - topY);
  if (availableH <= 0) return;

  copyMetadataBackgroundToSprite(g_quoteLineSprite, drawX, topY);
  g_quoteLineSprite->setFont(font);
  g_quoteLineSprite->setTextColor(colorClock);
  g_quoteLineSprite->setTextWrap(false);

  const int baselineInSprite = baselineY - topY;
  if (cycle > clipWidth) {
    const int textStartX = -g_quoteMarquee.offset;
    g_quoteLineSprite->setCursor(textStartX, baselineInSprite);
    g_quoteLineSprite->print(repeatedText);
    g_quoteLineSprite->setCursor(textStartX + g_quoteMarquee.cycleWidth, baselineInSprite);
    g_quoteLineSprite->print(repeatedText);
  } else {
    g_quoteLineSprite->setCursor(0, baselineInSprite);
    g_quoteLineSprite->print(encodedQuote);
  }

  gfx->draw16bitRGBBitmap(drawX, topY, (uint16_t*)g_quoteLineSprite->getFramebuffer(), clipWidth, availableH);
}
}

static bool shouldShowClock(ScreenId screen) {
  return screen == ScreenId::Clock1 || screen == ScreenId::Clock2;
}

static bool shouldShowVu(ScreenId screen) {
  return vuMeterEnabled && (screen == ScreenId::VuMeter1 || screen == ScreenId::VuMeter2 || screen == ScreenId::VuMeter3);
}

static bool shouldShowMetadata(ScreenId screen) {
  return screen == ScreenId::StationInfo;
}

static bool shouldShowPrimaryMarquee(ScreenId screen) {
  return primaryMarqueeEnabled && isScreenImplemented(screen);
}

static bool shouldShowSecondaryMarquee(ScreenId screen) {
  return secondaryMarqueeEnabled && isScreenImplemented(screen);
}

static bool shouldShowQuoteMarquee(ScreenId screen) {
  return quoteMarqueeEnabled && isScreenImplemented(screen) && screen != ScreenId::VuMeter3;
}

// static bool shouldShowStatusOverlay(ScreenId screen) {
//   return screen == ScreenId::Clock1;
// }

static int analogClockDiameter() {
  // Keep analog clock sprite compact so it tracks hand length more closely.
  return static_cast<int>(min(vuWidth, vuHeight) * 0.62f);
}

static void copyBackgroundToSprite(Arduino_Canvas* sprite, int spriteW, int spriteH, int drawX, int drawY) {
  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* spriteBuf = (uint16_t*)sprite->getFramebuffer();

  for (int y = 0; y < spriteH; y++) {
    int srcY = drawY + y;
    if (srcY >= 0 && srcY < vuHeight) {
      int safeX = drawX;
      if (safeX < 0 || safeX + spriteW > vuWidth) {
        for (int x = 0; x < spriteW; ++x) {
          int srcX = drawX + x;
          spriteBuf[y * spriteW + x] = (srcX >= 0 && srcX < vuWidth) ? bgBuf[srcY * vuWidth + srcX] : RGB565_BLACK;
        }
      } else {
        memcpy(&spriteBuf[y * spriteW], &bgBuf[srcY * vuWidth + drawX], spriteW * sizeof(uint16_t));
      }
    } else {
      memset(&spriteBuf[y * spriteW], 0, spriteW * sizeof(uint16_t));
    }
  }
}

static void drawClockHMSS();
static void drawVuNeedleChannel(Arduino_Canvas* needleSprite, int posX, int posY, int vuLevel, bool mirrored);

static bool isVuBarsScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter1;
}

static bool isVuNeedlesScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter2;
}

static bool isVuCardioScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter3;
}

static bool ensureVuCardioSprite(int width, int height) {
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

static void drawVuCardioWindow(bool advanceWaveform) {
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

  // БАЗОВАЯ ЛИНИЯ ТЕПЕРЬ ВНИЗУ (80% от высоты)
  const int bottomY = drawH - 8;  // Отступ снизу
  const int topY = 8;              // Отступ сверху
  const int usableH = drawH - topY - 8;  // Доступная высота для пульсации
  
  if (advanceWaveform && !g_vuCardioWaveform.empty()) {
    float rawLevel = max(current_needle_L, current_needle_R) / 100.0f;
    
    // Логарифмическое преобразование для лучшей видимости
    float displayLevel = pow(rawLevel, 0.5f);  // Корень - сильно растягивает тихие звуки
    
    const float basePulse = (radioStarted && !isPaused) ? displayLevel : 0.03f;
    const float jitter = (static_cast<float>(random(-100, 101)) / 100.0f) * (radioStarted && !isPaused ? 0.14f : 0.03f);
    const float pulse = constrain(basePulse * 1.15f + jitter, 0.0f, 1.0f);
    
    // Амплитуда пульсации (от bottomY вверх)
    const int amplitude = max(2, static_cast<int>(pulse * usableH));
    
    // Новая точка: от bottomY поднимаемся вверх
    int newPoint = bottomY - amplitude + random(-2, 2);
    newPoint = constrain(newPoint, topY, bottomY);
    
    for (size_t i = 1; i < g_vuCardioWaveform.size(); ++i) {
      g_vuCardioWaveform[i - 1] = g_vuCardioWaveform[i];
    }
    g_vuCardioWaveform.back() = static_cast<int16_t>(newPoint);
  }

  const uint16_t bgShade = RGB565(2, 12, 2);
  const uint16_t gridColor = RGB565(18, 55, 18);
  const uint16_t frameColor = RGB565(55, 110, 55);
  const uint16_t baselineColor = RGB565(58, 122, 58);
  const uint16_t glowColor = RGB565(70, 255, 70);
  const uint16_t traceColor = RGB565(62, 255, 62);
  const uint16_t dimTrace = RGB565(30, 120, 30);

  g_vuCardioSprite->fillRect(0, 0, drawW, drawH, bgShade);

  // Рисуем сетку
  for (int y = 0; y < drawH; y += 24) {
    g_vuCardioSprite->drawFastHLine(0, y, drawW, gridColor);
  }
  for (int x = 0; x < drawW; x += 32) {
    g_vuCardioSprite->drawFastVLine(x, 0, drawH, gridColor);
  }
  
  // БАЗОВАЯ ЛИНИЯ ТЕПЕРЬ ВНИЗУ
  g_vuCardioSprite->drawFastHLine(0, bottomY, drawW, baselineColor);

  if (g_vuCardioWaveform.size() != static_cast<size_t>(drawW)) {
    g_vuCardioWaveform.assign(drawW, bottomY);
  }

  // Рисуем кардио-линию
  for (int x = 1; x < drawW; ++x) {
    const int y0 = constrain(g_vuCardioWaveform[x - 1], 0, drawH - 1);
    const int y1 = constrain(g_vuCardioWaveform[x], 0, drawH - 1);
    g_vuCardioSprite->drawLine(x - 1, min(drawH - 1, y0 + 1), x, min(drawH - 1, y1 + 1), dimTrace);
    g_vuCardioSprite->drawLine(x - 1, y0, x, y1, glowColor);
    if ((x % 2) == 0) {
      g_vuCardioSprite->drawLine(x - 1, max(0, y0 - 1), x, max(0, y1 - 1), traceColor);
    }
  }

  g_vuCardioSprite->drawRect(0, 0, drawW, drawH, frameColor);
  gfx->draw16bitRGBBitmap(drawX, drawY, spriteBuf, drawW, drawH);
}


static void drawVuCardioWindowDynamic(bool advanceWaveform) {
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

      const float currentPeak = mono;
      const float currentRms = mono * mono;
      g_vuCardioPeak = max(currentPeak, g_vuCardioPeak * 0.92f);
      g_vuCardioRms = g_vuCardioRms * 0.88f + currentRms * 0.12f;

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
    const float jitterRange = (radioStarted && !isPaused)
                            ? (0.05f + shapedLevel * 0.07f)
                            : 0.03f;
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

  const uint16_t bgShade = RGB565(2, 12, 2);
  const uint16_t gridColor = RGB565(18, 55, 18);
  const uint16_t frameColor = RGB565(55, 110, 55);
  const uint16_t baselineColor = RGB565(58, 122, 58);
  const uint16_t glowColor = RGB565(70, 255, 70);
  const uint16_t traceColor = RGB565(62, 255, 62);
  const uint16_t dimTrace = RGB565(30, 120, 30);
  (void)bgShade;
  (void)gridColor;
  (void)frameColor;
  (void)baselineColor;

  if (g_vuCardioWaveform.size() != static_cast<size_t>(drawW)) {
    g_vuCardioWaveform.assign(drawW, centerY);
  }

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

static void drawVuBars() {
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
  const int botY = topY + outerH + gapY;

  const int innerPad = 2;
  const int innerX = drawX + innerPad;
  const int innerW = max(1, outerW - innerPad * 2);
  const int innerH = max(1, outerH - innerPad * 2);
  const int segments = constrain(vuBarsSegments, 4, 40);
  const int segGap = max(2, innerW / 120);
  const int segW = max(1, (innerW - segGap * (segments - 1)) / segments);

  const int blockY = topY;
  const int blockH = outerH * 2 + gapY;

  ensureVuBarsSprite(canvasW, canvasH);
  if (!g_vuBarsSprite) return;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* spriteBuf = (uint16_t*)g_vuBarsSprite->getFramebuffer();
  if (!bgBuf || !spriteBuf) return;

  for (int y = 0; y < blockH; ++y) {
    const int srcY = blockY + y;
    if (srcY < 0 || srcY >= canvasH) {
      memset(&spriteBuf[y * canvasW], 0, canvasW * sizeof(uint16_t));
      continue;
    }
    memcpy(&spriteBuf[y * canvasW], &bgBuf[srcY * vuWidth], canvasW * sizeof(uint16_t));
  }

  g_vuBarsSprite->setTextWrap(false);

  const unsigned long now = millis();
  if (g_vuPeakLastMs == 0) g_vuPeakLastMs = now;
  const unsigned long dt = now - g_vuPeakLastMs;
  g_vuPeakLastMs = now;

  const float decayPerSecond = 32.0f;
  const float decay = decayPerSecond * (static_cast<float>(dt) / 1000.0f);
  g_vuPeakL = max(0.0f, g_vuPeakL - decay);
  g_vuPeakR = max(0.0f, g_vuPeakR - decay);
  g_vuPeakL = max(g_vuPeakL, static_cast<float>(current_needle_L));
  g_vuPeakR = max(g_vuPeakR, static_cast<float>(current_needle_R));

  auto segmentColor = [&](int index) -> uint16_t {
    const float ratio = static_cast<float>(index + 1) / static_cast<float>(segments);
    if (ratio <= 0.65f) return RGB565_GREEN;
    if (ratio <= 0.85f) return RGB565_YELLOW;
    return RGB565_RED;
  };

  auto drawSegmentedBar = [&](int level, float peakLevel, int yInBlock) {
    const int clamped = constrain(level, 0, 100);
    const int activeSegments = map(clamped, 0, 100, 0, segments);
    const int peakSegment = constrain(map(static_cast<int>(peakLevel), 0, 100, 0, segments - 1), 0, segments - 1);

    // g_vuBarsSprite->drawRoundRect(drawX, yInBlock, outerW, outerH, 4, RGB565_WHITE); // Uncomment for a rounded border
    g_vuBarsSprite->fillRect(innerX, yInBlock + innerPad, innerW, innerH, RGB565_BLACK);

    for (int i = 0; i < segments; ++i) {
      const int sx = innerX + i * (segW + segGap);
      const int sy = yInBlock + innerPad;
      const uint16_t color = segmentColor(i);
      if (i < activeSegments) {
        g_vuBarsSprite->fillRect(sx, sy, segW, innerH, color);
      } else {
        g_vuBarsSprite->drawRect(sx, sy, segW, innerH, RGB565_GRAY);
      }
    }

    const int px = innerX + peakSegment * (segW + segGap);
    const int py = yInBlock + 1;
    g_vuBarsSprite->drawFastVLine(px + segW / 2, py, outerH - 2, RGB565_WHITE);
  };

  drawSegmentedBar(current_needle_L, g_vuPeakL, 0);
  drawSegmentedBar(current_needle_R, g_vuPeakR, outerH + gapY);

  gfx->draw16bitRGBBitmap(0, blockY, (uint16_t*)g_vuBarsSprite->getFramebuffer(), canvasW, blockH);
}

// static void drawStatusOverlay(ScreenId currentScreen) {
//   if (!shouldShowStatusOverlay(currentScreen)) return;

//   const char* statusText = nullptr;
//   uint16_t statusColor = RGB565_WHITE;
//   if (isPaused) {
//     statusText = "PAUSED";
//     statusColor = RGB565_ORANGE;
//   } else if (!radioStarted && !sleepTimer.isSleepMode()) {
//     statusText = "STOPPED";
//     statusColor = RGB565_RED;
//   } else if (metadataPending) {
//     statusText = "LOADING...";
//     statusColor = RGB565_YELLOW;
//   } else if (sleepTimer.isSleepMode()) {
//     statusText = "SLEEP";
//     statusColor = RGB565_WHITE;
//   }

//   if (!statusText) return;

//   gfx->setFont(&METADATA_FONT);
//   gfx->setTextColor(statusColor);
//   int16_t x1, y1;
//   uint16_t w, h;
//   gfx->getTextBounds(statusText, 0, 0, &x1, &y1, &w, &h);
//   gfx->setCursor((gfx->width() - w) / 2, 180);
//   gfx->print(statusText);
// }

static void renderScreenToCanvas(Arduino_Canvas* target, ScreenId currentScreen, bool resetMarquees, bool forceClockRedraw) {
  if (!target || !bgSprite || !gfx) return;

  uint16_t* srcBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* dstBuf = (uint16_t*)target->getFramebuffer();
  if (!srcBuf || !dstBuf) return;

  memcpy(dstBuf, srcBuf, static_cast<size_t>(vuWidth) * static_cast<size_t>(vuHeight) * sizeof(uint16_t));

  Arduino_GFX* physicalGfx = gfx;
  gfx = target;

  if (resetMarquees) {
    resetMetadataMarquees();
  }

  if (shouldShowMetadata(currentScreen)) {
    drawDataLines();
  } else {
    if (shouldShowPrimaryMarquee(currentScreen)) {
      drawPrimaryMarquee(false);
    }
    if (shouldShowSecondaryMarquee(currentScreen)) {
      drawSecondaryMarquees(false);
    }
  }
  if (shouldShowQuoteMarquee(currentScreen)) {
    drawQuoteMarquee(false);
  }

  // drawStatusOverlay(currentScreen);

  if (forceClockRedraw) {
    redrawClock = true;
  }
  if (shouldShowClock(currentScreen)) {
    drawClockHMSS();
  }
  if (shouldShowVu(currentScreen) && isVuBarsScreen(currentScreen)) {
    drawVuBars();
  } else if (shouldShowVu(currentScreen) && isVuNeedlesScreen(currentScreen)) {
    drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L, leftNeedleMirrored);
    drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R, false);
  } else if (shouldShowVu(currentScreen) && isVuCardioScreen(currentScreen)) {
    drawVuCardioWindowDynamic(false);
  }

  sleepTimer.drawIndicator(nullptr, gfx);

  if (shouldShowMetadata(currentScreen)) {
    drawVolumeDisplay();
  }

  gfx = physicalGfx;
}

void drawVolumeDisplay() {
#ifdef SHOW_VOLUME_DISPLAY
  if (SHOW_VOLUME_DISPLAY) {
    String text = "Vol: " + String(currentVolume);
    gfx->setFont(&CLOCK_FONT);
    
    // Вычислить размер текста
    int16_t x = 0, y = 0;
    uint16_t w = 0, h = 0;
    gfx->getTextBounds(text, volumeX, volumeY, &x, &y, &w, &h);
    
    // Закрасить фон серым (RGB565: 128,128,128 = 0x8410)
    const uint16_t grayColor = 0x8410;
    gfx->fillRect(x - 2, y - 2, w + 4, h + 4, grayColor);
    
    // Вывести текст
    gfx->setTextColor(colorVolume);
    gfx->setCursor(volumeX, volumeY);
    gfx->print(text);
  }
#endif
}

void initClock() {
  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
}

static void drawClockHMSS() {
  const ScreenId currentScreen = getCurrentScreen();
  if (!shouldShowClock(currentScreen)) return;

  static int lastSec = -1;
  const bool forceRedraw = redrawClock;
  
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  // Обновляем спрайт при смене секунды или при необходимости мигания
  unsigned long BlinkCheck = (BLINK_COLON) ? (millis() / 500) : (millis() / 1000);
  static unsigned long lastBlinkCheck = 0;
  
  if (!forceRedraw && timeinfo.tm_sec == lastSec && BlinkCheck == lastBlinkCheck) return;
  
  if (timeinfo.tm_sec != lastSec) lastSec = timeinfo.tm_sec;
  lastBlinkCheck = BlinkCheck;

  // Формируем строку времени в зависимости от конфига
  char clockBuf[10];
  
  if (SHOW_SECONDS) {
    sprintf(clockBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    sprintf(clockBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }

  if (currentScreen == ScreenId::Clock2) {
    int drawX = clockX;
    int drawY = clockY;

    copyBackgroundToSprite(analogClockSprite, analogClockW, analogClockH, drawX, drawY);

    const int diameter = analogClockDiameter();
    const int radius = diameter / 2;
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
    gfx->draw16bitRGBBitmap(drawX, drawY, (uint16_t*)analogClockSprite->getFramebuffer(), analogClockW, analogClockH);
    redrawClock = false;
    return;
  }

  copyBackgroundToSprite(clockSprite, clockW, clockH, clockSpriteX, clockY);
  clockSprite->setTextColor(colorClock);
  clockSprite->setFont(&CLOCK_FONT);

  int16_t x = 0, y = 0;
  uint16_t w = 0, h = 0;
  
  // Если мигание включено - меняем двоеточие на точку-запятую
  char timeDisplay[10];
  if (BLINK_COLON) {
    if ((millis() / 500) % 2 == 0) {
      // Первая половина - двоеточие
      sprintf(timeDisplay, "%s", clockBuf);
    } else {
      // Вторая половина - точка-запятая вместо двоеточия
      if (SHOW_SECONDS) {
        sprintf(timeDisplay, "%02d;%02d;%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      } else {
        sprintf(timeDisplay, "%02d;%02d", timeinfo.tm_hour, timeinfo.tm_min);
      }
    }
  } else {
    sprintf(timeDisplay, "%s", clockBuf);
  }
  
  clockSprite->getTextBounds(timeDisplay, 0, 0, &x, &y, &w, &h);
  int drawX = (clockW - w) / 2;
  int drawY = (clockH + h) / 2;
  
  clockSprite->setCursor(drawX, drawY);
  clockSprite->print(timeDisplay);
  
  gfx->draw16bitRGBBitmap(clockSpriteX, clockY, (uint16_t*)clockSprite->getFramebuffer(), clockW, clockH);
  redrawClock = false;
}

static void updateVULevels() {
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

void createSprites() {
  vuWidth = gfx->width();
  vuHeight = gfx->height();
  int W = constrain(needleSpriteWidth, 16, vuWidth);
  int H = constrain(needleSpriteHeight, 16, vuHeight);
  needleSpriteWidth = W;
  needleSpriteHeight = H;

  bgSprite = createManagedCanvas(vuWidth, vuHeight, gfx, true, "bgSprite");
  g_sceneSprite = createManagedCanvas(vuWidth, vuHeight, gfx, true, "sceneSprite");

  needleSpriteL = createManagedCanvas(W, H, gfx, false, "needleSpriteL");
  needleSpriteR = createManagedCanvas(W, H, gfx, false, "needleSpriteR");

  gfx->setFont(&CLOCK_FONT);
  char testStr[] = "88:88:88";
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(testStr, 0, 0, &x1, &y1, &w, &h);
  clockW = w + 6;
  clockH = h + 6;
  const int analogDiameter = analogClockDiameter();
  analogClockW = analogDiameter + 2;
  analogClockH = analogDiameter + 2;

  clockSprite = createManagedCanvas(clockW, clockH, gfx, true, "clockSprite");
  analogClockSprite = createManagedCanvas(analogClockW, analogClockH, gfx, true, "analogClockSprite");
  if (!bgSprite || !needleSpriteL || !needleSpriteR || !clockSprite || !analogClockSprite) {
    Serial.println("Sprite allocation failed");
    return;
  }
  if (!g_sceneSprite) {
    Serial.println("Scene sprite allocation failed, using direct redraw");
  }

  clockSpriteX = clockX;
  clockSpriteY = clockY;
  clockX = clockSpriteX;
  clockY = clockSpriteY;

  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* clkBuf = (uint16_t*)clockSprite->getFramebuffer();
  for (int y = 0; y < clockH; y++) {
    int srcY = clockSpriteY + y;
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

static bool tpgOutputToSprite(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= vuHeight) return false;
  bgSprite->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void loadBackground() {
  //Serial.printf("Loading: %s\n", currentBackground.c_str());
  if (!LittleFS.exists(currentBackground)) {
    bgSprite->fillScreen(RGB565_BLACK);
    return;
  }

  File file = LittleFS.open(currentBackground, "r");
  if (!file) {
    bgSprite->fillScreen(RGB565_BLACK);
    return;
  }

  size_t fileSize = file.size();
  uint8_t* jpegData = (uint8_t*)ps_malloc(fileSize);
  if (!jpegData) {
    file.close();
    bgSprite->fillScreen(RGB565_BLACK);
    return;
  }

  file.read(jpegData, fileSize);
  file.close();

  uint16_t imgW, imgH;
  TJpgDec.getJpgSize(&imgW, &imgH, jpegData, fileSize);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tpgOutputToSprite);
  TJpgDec.drawJpg(0, 0, jpegData, fileSize);
  free(jpegData);

  if (!sleepTimer.isSleepMode() && sleepTimer.isTimerActive()) {
    bgSprite->fillCircle(sleepTimer.getIndicatorX(), sleepTimer.getIndicatorY(), 6, RGB565_BLACK);
  }
}

static void drawVuNeedleChannel(Arduino_Canvas* needleSprite, int posX, int posY, int vuLevel, bool mirrored) {
  int W = needleSprite->width();
  int H = needleSprite->height();
  uint16_t* needleBuf = (uint16_t*)needleSprite->getFramebuffer();
  copyBackgroundToSprite(needleSprite, W, H, posX, posY);

  int cx = W / 2;
  int cy = needleCY;
  float angle = map(vuLevel, 0, 100, needleMinAngle, needleMaxAngle);
  float rad = angle * 3.14159f / 180.0f;
  float cosA = cos(rad);
  float sinA = sin(rad);
  const float originX = (W - 1) * 0.5f;
  const float originY = (H - 1) * 0.5f;
  const int rotation = ((needleRotation % 360) + 360) % 360;
  auto transformPoint = [W, H, mirrored, originX, originY, rotation](int x, int y, int& outX, int& outY) {
    float px = static_cast<float>(x);
    float py = static_cast<float>(mirrored ? (H - 1 - y) : y);
    float dx = px - originX;
    float dy = py - originY;
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
    int offsetX = t * sin(rad);
    int offsetY = -t * cos(rad);
    int x1, y1, x2, y2, x3, y3;
    transformPoint(cx + offsetX, cy + offsetY, x1, y1);
    transformPoint(cx + (needleLenMain - needleLenRed) * cosA + offsetX,
                   cy + (needleLenMain - needleLenRed) * sinA + offsetY, x2, y2);
    transformPoint(cx + needleLenMain * cosA + offsetX,
                   cy + needleLenMain * sinA + offsetY, x3, y3);
    needleSprite->drawLine(x1, y1, x2, y2, colorNeedleMain);
    needleSprite->drawLine(x2, y2, x3, y3, colorNeedleRed);
  }

  int pivotX, pivotY;
  transformPoint(cx, cy, pivotX, pivotY);
  needleSprite->fillCircle(pivotX, pivotY, 3, RGB565(0xAD, 0x55, 0x55));
  needleSprite->fillCircle(pivotX, pivotY, 1, colorNeedleRed);
  gfx->draw16bitRGBBitmap(posX, posY, needleBuf, W, H);

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

void displayTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(500));
  int lastDrawnNeedleL = -1;
  int lastDrawnNeedleR = -1;
  ScreenId lastScreen = getCurrentScreen();
  while (true) {
    if (g_displayTaskPaused) {
      vTaskDelay(pdMS_TO_TICKS(DISPLAY_TASK_DELAY));
      continue;
    }
    const ScreenId currentScreen = getCurrentScreen();
    const unsigned long now = millis();
    if (quoteMarqueeEnabled) {
      updateClockQuoteIfNeeded();
    }
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      if (bgSprite != nullptr && needleSpriteL != nullptr && needleSpriteR != nullptr) {
        const bool screenChanged = currentScreen != lastScreen;
        if (shouldShowVu(currentScreen)) {
          updateVULevels();
          if (isVuBarsScreen(currentScreen)) {
            drawVuBars();
          } else if (isVuNeedlesScreen(currentScreen)) {
            if (screenChanged || current_needle_L != lastDrawnNeedleL) {
              drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L, leftNeedleMirrored);
              lastDrawnNeedleL = current_needle_L;
            }
            if (screenChanged || current_needle_R != lastDrawnNeedleR) {
              drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R, false);
              lastDrawnNeedleR = current_needle_R;
            }
          } else if (isVuCardioScreen(currentScreen)) {
            drawVuCardioWindowDynamic(true);
          }
        } else if (screenChanged) {
          lastDrawnNeedleL = -1;
          lastDrawnNeedleR = -1;
        }
        if (shouldShowClock(currentScreen)) {
          drawClockHMSS();
        }
        sleepTimer.drawIndicator(nullptr, gfx);
      }
      lastScreen = currentScreen;
      xSemaphoreGive(xMutex);
    }
    if ((shouldShowMetadata(currentScreen) || shouldShowPrimaryMarquee(currentScreen) || shouldShowSecondaryMarquee(currentScreen) || shouldShowQuoteMarquee(currentScreen)) &&
        now - g_lastMetadataFrameMs >= kMetadataFrameIntervalMs) {
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_lastMetadataFrameMs = now;
        if (shouldShowMetadata(currentScreen)) {
          drawMetadataMarquees(true);
        } else {
          if (shouldShowPrimaryMarquee(currentScreen)) {
            drawPrimaryMarquee(true);
          }
          if (shouldShowSecondaryMarquee(currentScreen)) {
            drawSecondaryMarquees(true);
          }
        }
        if (shouldShowQuoteMarquee(currentScreen)) {
          drawQuoteMarquee(true);
        }
        xSemaphoreGive(xMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_TASK_DELAY));
  }
}

void redrawScreenLocked() {
  const ScreenId currentScreen = getCurrentScreen();
  const unsigned long now = millis();

  prevStreamName = currentStreamName;
  if (g_sceneSprite) {
    renderScreenToCanvas(g_sceneSprite, currentScreen, true, true);
    if (shouldShowMetadata(currentScreen) || shouldShowPrimaryMarquee(currentScreen) || shouldShowSecondaryMarquee(currentScreen) || shouldShowQuoteMarquee(currentScreen)) {
      g_lastMetadataFrameMs = now;
    }
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)g_sceneSprite->getFramebuffer(), vuWidth, vuHeight);
    return;
  }

  resetMetadataMarquees();
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)bgSprite->getFramebuffer(), vuWidth, vuHeight);
  if (shouldShowMetadata(currentScreen)) {
    drawDataLines();
  } else {
    if (shouldShowPrimaryMarquee(currentScreen)) {
      drawPrimaryMarquee(false);
    }
    if (shouldShowSecondaryMarquee(currentScreen)) {
      drawSecondaryMarquees(false);
    }
  }
  if (shouldShowQuoteMarquee(currentScreen)) {
    drawQuoteMarquee(false);
  }

  redrawClock = true;
  if (shouldShowClock(currentScreen)) {
    drawClockHMSS();
  }
  if (shouldShowVu(currentScreen) && isVuBarsScreen(currentScreen)) {
    drawVuBars();
  } else if (shouldShowVu(currentScreen) && isVuNeedlesScreen(currentScreen)) {
    drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L, leftNeedleMirrored);
    drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R, false);
  } else if (shouldShowVu(currentScreen) && isVuCardioScreen(currentScreen)) {
    drawVuCardioWindowDynamic(false);
  }
  sleepTimer.drawIndicator(nullptr, gfx);
  if (shouldShowMetadata(currentScreen)) {
    drawVolumeDisplay();
    g_lastMetadataFrameMs = now;
  } else if (shouldShowPrimaryMarquee(currentScreen) || shouldShowSecondaryMarquee(currentScreen) || shouldShowQuoteMarquee(currentScreen)) {
    g_lastMetadataFrameMs = now;
  }
}

void redrawScreen() {
  const ScreenId currentScreen = getCurrentScreen();

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    redrawScreenLocked();
    xSemaphoreGive(xMutex);
    return;
    prevStreamName = currentStreamName;
    if (g_sceneSprite) {
      renderScreenToCanvas(g_sceneSprite, currentScreen, true, true);
      gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)g_sceneSprite->getFramebuffer(), vuWidth, vuHeight);
      xSemaphoreGive(xMutex);
      return;
    }
    resetMetadataMarquees();
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)bgSprite->getFramebuffer(), vuWidth, vuHeight);
    if (shouldShowMetadata(currentScreen)) {
      drawDataLines();
    }

    prevStreamName = currentStreamName;
    redrawClock = true;
    if (shouldShowClock(currentScreen)) {
      drawClockHMSS();
      if (currentScreen == ScreenId::Clock1) {
        // drawClockQuote(); //вызов цитат
      }
    }
    if (shouldShowVu(currentScreen)) {
      if (isVuNeedlesScreen(currentScreen)) {
        drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L, leftNeedleMirrored);
        drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R, false);
      } else if (isVuCardioScreen(currentScreen)) {
        drawVuCardioWindowDynamic(false);
      } else if (isVuBarsScreen(currentScreen)) {
        drawVuBars();
      }
    }
    sleepTimer.drawIndicator(nullptr, gfx);
    if (shouldShowMetadata(currentScreen)) {
      drawVolumeDisplay();
    }
    xSemaphoreGive(xMutex);
  }
}

void refreshVuOnly() {
  const ScreenId currentScreen = getCurrentScreen();
  if (!shouldShowVu(currentScreen)) return;

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    return;
  }

  if (shouldShowMetadata(currentScreen) || shouldShowClock(currentScreen)) {
    redrawScreenLocked();
    xSemaphoreGive(xMutex);
    return;
  }

  if (shouldShowVu(currentScreen) && isVuBarsScreen(currentScreen)) {
    drawVuBars();
  } else if (needleSpriteL && needleSpriteR && shouldShowVu(currentScreen) && isVuNeedlesScreen(currentScreen)) {
    drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L, leftNeedleMirrored);
    drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R, false);
  } else if (shouldShowVu(currentScreen) && isVuCardioScreen(currentScreen)) {
    drawVuCardioWindowDynamic(true);
  }

  xSemaphoreGive(xMutex);
}

void setDisplayTaskPaused(bool paused) {
  g_displayTaskPaused = paused;
}
