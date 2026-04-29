#include "display_manager.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Audio.h>

#include "config.h"
#include "display_text_utils.h"
#include "display_widgets.h"
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
Arduino_Canvas* g_quoteLineSprite = nullptr;
MarqueeLineState g_marqueeLines[3];
MarqueeLineState g_quoteMarquee;
unsigned long g_lastMetadataFrameMs = 0;
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

static bool isVuBarsScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter1;
}

static bool isVuNeedlesScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter2;
}

static bool isVuCardioScreen(ScreenId screen) {
  return screen == ScreenId::VuMeter3;
}

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
  initWidgetClock();
}

void createSprites() {
  createWidgetSprites();
  g_sceneSprite = createManagedCanvas(vuWidth, vuHeight, gfx, true, "sceneSprite");
  if (!g_sceneSprite) {
    Serial.println("Scene sprite allocation failed, using direct redraw");
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
