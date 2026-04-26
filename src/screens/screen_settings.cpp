#include "screen_settings.h"

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <WString.h>

#include "config.h"
#include "display_manager.h"
#include "screen_manager.h"
#include "screens.h"

#ifndef DISPLAY_PROFILE_NEEDLE_ROTATION
#define DISPLAY_PROFILE_NEEDLE_ROTATION 0
#endif

extern String currentBackground;
extern int needlePosLX;
extern int needlePosLY;
extern int needlePosRX;
extern int needlePosRY;
extern int needleThickness;
extern int needleLenMain;
extern int needleLenRed;
extern int needleCY;
extern int needleSpriteWidth;
extern int needleSpriteHeight;
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern bool primaryMarqueeEnabled;
extern bool secondaryMarqueeEnabled;
extern bool quoteMarqueeEnabled;
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
extern int clockX;
extern int clockY;
extern int clockSpriteX;
extern int clockSpriteY;
extern int volumeX;
extern int volumeY;
extern int ipX;
extern int ipY;
extern int quoteX;
extern int quoteY;
extern int quoteW;
extern int vuBarsHeight;
extern int vuBarsSegments;
extern int vuBarsOffsetX;
extern int vuBarsOffsetY;
extern int vuCardioX;
extern int vuCardioY;
extern int vuCardioW;
extern int vuCardioH;
extern int needleMinAngle;
extern int needleMaxAngle;
extern int needleUpSpeed;
extern int needleDownSpeed;
extern int needleRotation;
extern uint16_t colorStation;
extern uint16_t colorTitle;
extern uint16_t colorBitrate;
extern uint16_t colorVolume;
extern uint16_t colorClock;
extern uint16_t colorNeedleMain;
extern uint16_t colorNeedleRed;
extern uint16_t colorIP;
extern String quoteApiUrl;
extern Arduino_GFX* gfx;

namespace {
String getScreenPrefix(ScreenId id) {
  switch (id) {
    case ScreenId::Clock1: return "c1";
    case ScreenId::Clock2: return "c2";
    case ScreenId::VuMeter1: return "v1";
    case ScreenId::VuMeter2: return "v2";
    case ScreenId::VuMeter3: return "v3";
    case ScreenId::StationInfo: return "s1";
    default: return "xx";
  }
}

String getShortSuffix(const char* suffix) {
  String key(suffix);
  if (key == "bg") return "bg";
  if (key == "needleLX") return "lx";
  if (key == "needleLY") return "ly";
  if (key == "needleRX") return "rx";
  if (key == "needleRY") return "ry";
  if (key == "needleThk") return "th";
  if (key == "needleMain") return "nm";
  if (key == "needleRed") return "nr";
  if (key == "needleCY") return "cy";
  if (key == "needleDbg") return "db";
  if (key == "needleMirrorL") return "ml";
  if (key == "needleSpriteW") return "sw";
  if (key == "needleSpriteH") return "sh";
  if (key == "needleMinAngle") return "mina";
  if (key == "needleMaxAngle") return "maxa";
  if (key == "needleUpSpeed") return "ups";
  if (key == "needleDownSpeed") return "dns";
  if (key == "needleRotation") return "rot";
  if (key == "stationX") return "sx";
  if (key == "stationY") return "sy";
  if (key == "title1X") return "t1x";
  if (key == "title1Y") return "t1y";
  if (key == "title2X") return "t2x";
  if (key == "title2Y") return "t2y";
  if (key == "stationW") return "swt";
  if (key == "title1W") return "w1";
  if (key == "title2W") return "w2";
  if (key == "marqueePauseMs") return "mp";
  if (key == "bitrateX") return "bx";
  if (key == "bitrateY") return "by";
  if (key == "clockX") return "cx";
  if (key == "clockY") return "cyc";
  if (key == "volumeX") return "vx";
  if (key == "volumeY") return "vy";
  if (key == "ipX") return "ix";
  if (key == "ipY") return "iy";
  if (key == "colorStation") return "cs";
  if (key == "colorTitle") return "ct";
  if (key == "colorBitrate") return "cb";
  if (key == "colorVolume") return "cv";
  if (key == "colorClock") return "cc";
  if (key == "colorNeedleMain") return "cnm";
  if (key == "colorNeedleRed") return "cnr";
  if (key == "colorIP") return "cip";
  if (key == "marquee1") return "m1";
  if (key == "marquee2") return "m2";
  if (key == "quoteEnabled") return "qe";
  if (key == "quoteX") return "qx";
  if (key == "quoteY") return "qy";
  if (key == "quoteW") return "qw";
  if (key == "vuBarsHeight") return "vbh";
  if (key == "vuBarsSegments") return "vbs";
  if (key == "vuBarsOffsetX") return "vbx";
  if (key == "vuBarsOffsetY") return "vby";
  if (key == "vuCardioX") return "vcx";
  if (key == "vuCardioY") return "vcy";
  if (key == "vuCardioW") return "vcw";
  if (key == "vuCardioH") return "vch";
  if (key == "quoteUrl") return "qu";
  return key;
}

String makeKey(ScreenId id, const char* suffix) {
  String key = getScreenPrefix(id) + "_" + getShortSuffix(suffix);
  if (key.length() > 15) key = key.substring(0, 15);
  return key;
}

String makeLegacyScreenKey(ScreenId id, const char* suffix) {
  return String(getScreenKey(id)) + "_" + suffix;
}

int getLegacyIntDefault(const char* key, int fallback);
uint16_t getLegacyColorDefault(const char* key, uint16_t fallback);
String getLegacyStringDefault(const char* key, const char* fallback);

int getScreenIntDefault(Preferences& prefs, ScreenId id, const char* suffix, int fallback) {
  const int missing = -32768;
  int value = prefs.getInt(makeKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  value = prefs.getInt(makeLegacyScreenKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  return getLegacyIntDefault(suffix, fallback);
}

uint16_t getScreenUShortDefault(Preferences& prefs, ScreenId id, const char* suffix, uint16_t fallback) {
  const uint16_t missing = 0xFFFF;
  uint16_t value = prefs.getUShort(makeKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  value = prefs.getUShort(makeLegacyScreenKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  return getLegacyColorDefault(suffix, fallback);
}

int getLegacyIntDefault(const char* key, int fallback) {
  Preferences prefs;
  prefs.begin("radio", true);
  const int value = prefs.getInt(key, fallback);
  prefs.end();
  return value;
}

uint16_t getLegacyColorDefault(const char* key, uint16_t fallback) {
  Preferences prefs;
  prefs.begin("radio", true);
  const uint16_t value = prefs.getUShort(key, fallback);
  prefs.end();
  return value;
}

String getLegacyStringDefault(const char* key, const char* fallback) {
  Preferences prefs;
  prefs.begin("radio", true);
  const String value = prefs.getString(key, fallback);
  prefs.end();
  return value;
}

String getScreenStringDefault(Preferences& prefs, ScreenId id, const char* suffix, const char* fallback) {
  const String missing = "__NO_VAL__";
  String value = prefs.getString(makeKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  value = prefs.getString(makeLegacyScreenKey(id, suffix).c_str(), missing);
  if (value != missing) return value;
  return getLegacyStringDefault(suffix, fallback);
}
}

void saveCurrentScreenSettings() {
  const ScreenId id = getCurrentScreen();
  Preferences prefs;
  prefs.begin("radio", false);

  const String bgKey = makeKey(id, "bg");
  prefs.putString(bgKey.c_str(), currentBackground);
  Serial.printf("[screen-settings] save bg screen=%s key=%s value=%s\n",
                getScreenKey(id), bgKey.c_str(), currentBackground.c_str());
  prefs.putInt(makeKey(id, "needleLX").c_str(), needlePosLX);
  prefs.putInt(makeKey(id, "needleLY").c_str(), needlePosLY);
  prefs.putInt(makeKey(id, "needleRX").c_str(), needlePosRX);
  prefs.putInt(makeKey(id, "needleRY").c_str(), needlePosRY);
  prefs.putInt(makeKey(id, "needleThk").c_str(), needleThickness);
  prefs.putInt(makeKey(id, "needleMain").c_str(), needleLenMain);
  prefs.putInt(makeKey(id, "needleRed").c_str(), needleLenRed);
  prefs.putInt(makeKey(id, "needleCY").c_str(), needleCY);
  prefs.putInt(makeKey(id, "needleDbg").c_str(), debugBordersEnabled ? 1 : 0);
  prefs.putInt(makeKey(id, "needleMirrorL").c_str(), leftNeedleMirrored ? 1 : 0);
  prefs.putInt(makeKey(id, "needleSpriteW").c_str(), needleSpriteWidth);
  prefs.putInt(makeKey(id, "needleSpriteH").c_str(), needleSpriteHeight);
  prefs.putInt(makeKey(id, "needleMinAngle").c_str(), needleMinAngle);
  prefs.putInt(makeKey(id, "needleMaxAngle").c_str(), needleMaxAngle);
  prefs.putInt(makeKey(id, "needleUpSpeed").c_str(), needleUpSpeed);
  prefs.putInt(makeKey(id, "needleDownSpeed").c_str(), needleDownSpeed);
  prefs.putInt(makeKey(id, "needleRotation").c_str(), needleRotation);

  prefs.putInt(makeKey(id, "stationX").c_str(), stationNameX);
  prefs.putInt(makeKey(id, "stationY").c_str(), stationNameY);
  prefs.putInt(makeKey(id, "title1X").c_str(), streamTitle1X);
  prefs.putInt(makeKey(id, "title1Y").c_str(), streamTitle1Y);
  prefs.putInt(makeKey(id, "title2X").c_str(), streamTitle2X);
  prefs.putInt(makeKey(id, "title2Y").c_str(), streamTitle2Y);
  prefs.putInt(makeKey(id, "stationW").c_str(), stationScrollWidth);
  prefs.putInt(makeKey(id, "title1W").c_str(), title1ScrollWidth);
  prefs.putInt(makeKey(id, "title2W").c_str(), title2ScrollWidth);
  prefs.putInt(makeKey(id, "marqueePauseMs").c_str(), marqueePauseMs);
  prefs.putInt(makeKey(id, "bitrateX").c_str(), bitrateX);
  prefs.putInt(makeKey(id, "bitrateY").c_str(), bitrateY);
  prefs.putInt(makeKey(id, "clockX").c_str(), clockX);
  prefs.putInt(makeKey(id, "clockY").c_str(), clockY);
  prefs.putInt(makeKey(id, "volumeX").c_str(), volumeX);
  prefs.putInt(makeKey(id, "volumeY").c_str(), volumeY);
  prefs.putInt(makeKey(id, "ipX").c_str(), ipX);
  prefs.putInt(makeKey(id, "ipY").c_str(), ipY);

  prefs.putUShort(makeKey(id, "colorStation").c_str(), colorStation);
  prefs.putUShort(makeKey(id, "colorTitle").c_str(), colorTitle);
  prefs.putUShort(makeKey(id, "colorBitrate").c_str(), colorBitrate);
  prefs.putUShort(makeKey(id, "colorVolume").c_str(), colorVolume);
  prefs.putUShort(makeKey(id, "colorClock").c_str(), colorClock);
  prefs.putUShort(makeKey(id, "colorNeedleMain").c_str(), colorNeedleMain);
  prefs.putUShort(makeKey(id, "colorNeedleRed").c_str(), colorNeedleRed);
  prefs.putUShort(makeKey(id, "colorIP").c_str(), colorIP);
  prefs.putInt(makeKey(id, "marquee1").c_str(), primaryMarqueeEnabled ? 1 : 0);
  prefs.putInt(makeKey(id, "marquee2").c_str(), secondaryMarqueeEnabled ? 1 : 0);
  prefs.putInt(makeKey(id, "quoteEnabled").c_str(), quoteMarqueeEnabled ? 1 : 0);
  prefs.putInt(makeKey(id, "quoteX").c_str(), quoteX);
  prefs.putInt(makeKey(id, "quoteY").c_str(), quoteY);
  prefs.putInt(makeKey(id, "quoteW").c_str(), quoteW);
  prefs.putInt(makeKey(id, "vuBarsHeight").c_str(), vuBarsHeight);
  prefs.putInt(makeKey(id, "vuBarsSegments").c_str(), vuBarsSegments);
  prefs.putInt(makeKey(id, "vuBarsOffsetX").c_str(), vuBarsOffsetX);
  prefs.putInt(makeKey(id, "vuBarsOffsetY").c_str(), vuBarsOffsetY);
  prefs.putInt(makeKey(id, "vuCardioX").c_str(), vuCardioX);
  prefs.putInt(makeKey(id, "vuCardioY").c_str(), vuCardioY);
  prefs.putInt(makeKey(id, "vuCardioW").c_str(), vuCardioW);
  prefs.putInt(makeKey(id, "vuCardioH").c_str(), vuCardioH);
  prefs.putString(makeKey(id, "quoteUrl").c_str(), quoteApiUrl);
  prefs.end();
}

void loadCurrentScreenSettings() {
  const ScreenId id = getCurrentScreen();
  Preferences prefs;
  prefs.begin("radio", true);

  const String bgKey = makeKey(id, "bg");
  currentBackground = getScreenStringDefault(prefs, id, "bg", DISPLAY_PROFILE_DEFAULT_BACKGROUND);
  Serial.printf("[screen-settings] load bg screen=%s key=%s value=%s\n",
                getScreenKey(id), bgKey.c_str(), currentBackground.c_str());
  needlePosLX = getScreenIntDefault(prefs, id, "needleLX", DISPLAY_PROFILE_NEEDLE_L_X);
  needlePosLY = getScreenIntDefault(prefs, id, "needleLY", DISPLAY_PROFILE_NEEDLE_L_Y);
  needlePosRX = getScreenIntDefault(prefs, id, "needleRX", DISPLAY_PROFILE_NEEDLE_R_X);
  needlePosRY = getScreenIntDefault(prefs, id, "needleRY", DISPLAY_PROFILE_NEEDLE_R_Y);
  needleThickness = getScreenIntDefault(prefs, id, "needleThk", DISPLAY_PROFILE_NEEDLE_THICKNESS);
  needleLenMain = getScreenIntDefault(prefs, id, "needleMain", DISPLAY_PROFILE_NEEDLE_MAIN_LEN);
  needleLenRed = getScreenIntDefault(prefs, id, "needleRed", DISPLAY_PROFILE_NEEDLE_RED_LEN);
  needleCY = getScreenIntDefault(prefs, id, "needleCY", DISPLAY_PROFILE_NEEDLE_CENTER_Y);
  debugBordersEnabled = getScreenIntDefault(prefs, id, "needleDbg", DEBUG_BORDERS ? 1 : 0) != 0;
  leftNeedleMirrored = getScreenIntDefault(prefs, id, "needleMirrorL", 0) != 0;
  needleSpriteWidth = getScreenIntDefault(prefs, id, "needleSpriteW", SPRITE_WIDTH);
  needleSpriteHeight = getScreenIntDefault(prefs, id, "needleSpriteH", SPRITE_HEIGHT);
  needleMinAngle = getScreenIntDefault(prefs, id, "needleMinAngle", DISPLAY_PROFILE_NEEDLE_MIN_ANGLE);
  needleMaxAngle = getScreenIntDefault(prefs, id, "needleMaxAngle", DISPLAY_PROFILE_NEEDLE_MAX_ANGLE);
  needleUpSpeed = getScreenIntDefault(prefs, id, "needleUpSpeed", DISPLAY_PROFILE_NEEDLE_UP_SPEED);
  needleDownSpeed = getScreenIntDefault(prefs, id, "needleDownSpeed", DISPLAY_PROFILE_NEEDLE_DOWN_SPEED);
  needleRotation = getScreenIntDefault(prefs, id, "needleRotation", DISPLAY_PROFILE_NEEDLE_ROTATION);

  stationNameX = getScreenIntDefault(prefs, id, "stationX", DISPLAY_PROFILE_STATION_X);
  stationNameY = getScreenIntDefault(prefs, id, "stationY", DISPLAY_PROFILE_STATION_Y);
  streamTitle1X = getScreenIntDefault(prefs, id, "title1X", DISPLAY_PROFILE_TITLE1_X);
  streamTitle1Y = getScreenIntDefault(prefs, id, "title1Y", DISPLAY_PROFILE_TITLE1_Y);
  streamTitle2X = getScreenIntDefault(prefs, id, "title2X", DISPLAY_PROFILE_TITLE2_X);
  streamTitle2Y = getScreenIntDefault(prefs, id, "title2Y", DISPLAY_PROFILE_TITLE2_Y);
  const int legacyMetadataWidth = getScreenIntDefault(prefs, id, "metadataW", 0);
  stationScrollWidth = getScreenIntDefault(prefs, id, "stationW", 0);
  title1ScrollWidth = getScreenIntDefault(prefs, id, "title1W", 0);
  title2ScrollWidth = getScreenIntDefault(prefs, id, "title2W", 0);
  marqueePauseMs = constrain(getScreenIntDefault(prefs, id, "marqueePauseMs", marqueePauseMs), 0, 10000);
  bitrateX = getScreenIntDefault(prefs, id, "bitrateX", DISPLAY_PROFILE_BITRATE_X);
  bitrateY = getScreenIntDefault(prefs, id, "bitrateY", DISPLAY_PROFILE_BITRATE_Y);
  clockX = getScreenIntDefault(prefs, id, "clockX", DISPLAY_PROFILE_CLOCK_X);
  clockY = getScreenIntDefault(prefs, id, "clockY", DISPLAY_PROFILE_CLOCK_Y);
  clockSpriteX = clockX;
  clockSpriteY = clockY;
  volumeX = getScreenIntDefault(prefs, id, "volumeX", DISPLAY_PROFILE_VOLUME_X);
  volumeY = getScreenIntDefault(prefs, id, "volumeY", DISPLAY_PROFILE_VOLUME_Y);
  ipX = getScreenIntDefault(prefs, id, "ipX", DISPLAY_PROFILE_IP_X);
  ipY = getScreenIntDefault(prefs, id, "ipY", DISPLAY_PROFILE_IP_Y);

  colorStation = getScreenUShortDefault(prefs, id, "colorStation", DISPLAY_PROFILE_COLOR_STATION);
  colorTitle = getScreenUShortDefault(prefs, id, "colorTitle", DISPLAY_PROFILE_COLOR_TITLE);
  colorBitrate = getScreenUShortDefault(prefs, id, "colorBitrate", DISPLAY_PROFILE_COLOR_BITRATE);
  colorVolume = getScreenUShortDefault(prefs, id, "colorVolume", DISPLAY_PROFILE_COLOR_VOLUME);
  colorClock = getScreenUShortDefault(prefs, id, "colorClock", DISPLAY_PROFILE_COLOR_CLOCK);
  colorNeedleMain = getScreenUShortDefault(prefs, id, "colorNeedleMain", DISPLAY_PROFILE_COLOR_NEEDLE_MAIN);
  colorNeedleRed = getScreenUShortDefault(prefs, id, "colorNeedleRed", DISPLAY_PROFILE_COLOR_NEEDLE_RED);
  colorIP = getScreenUShortDefault(prefs, id, "colorIP", DISPLAY_PROFILE_COLOR_IP);
  primaryMarqueeEnabled = getScreenIntDefault(prefs, id, "marquee1", 1) != 0;
  secondaryMarqueeEnabled = getScreenIntDefault(prefs, id, "marquee2", primaryMarqueeEnabled ? 1 : 0) != 0;
  quoteMarqueeEnabled = getScreenIntDefault(prefs, id, "quoteEnabled", quoteMarqueeEnabled ? 1 : 0) != 0;
  quoteX = getScreenIntDefault(prefs, id, "quoteX", quoteX);
  quoteY = getScreenIntDefault(prefs, id, "quoteY", quoteY);
  quoteW = getScreenIntDefault(prefs, id, "quoteW", quoteW);
  vuBarsHeight = constrain(getScreenIntDefault(prefs, id, "vuBarsHeight", vuBarsHeight), 6, 120);
  vuBarsSegments = constrain(getScreenIntDefault(prefs, id, "vuBarsSegments", vuBarsSegments), 4, 40);
  vuBarsOffsetX = constrain(getScreenIntDefault(prefs, id, "vuBarsOffsetX", vuBarsOffsetX), -200, 200);
  vuBarsOffsetY = constrain(getScreenIntDefault(prefs, id, "vuBarsOffsetY", vuBarsOffsetY), -200, 200);
  vuCardioX = getScreenIntDefault(prefs, id, "vuCardioX", vuCardioX);
  vuCardioY = getScreenIntDefault(prefs, id, "vuCardioY", vuCardioY);
  vuCardioW = constrain(getScreenIntDefault(prefs, id, "vuCardioW", vuCardioW), 80, 480);
  vuCardioH = constrain(getScreenIntDefault(prefs, id, "vuCardioH", vuCardioH), 40, 320);
  quoteApiUrl = getScreenStringDefault(prefs, id, "quoteUrl", quoteApiUrl.c_str());
  prefs.end();
  if (stationScrollWidth <= 0) stationScrollWidth = legacyMetadataWidth;
  if (title1ScrollWidth <= 0) title1ScrollWidth = legacyMetadataWidth;
  if (title2ScrollWidth <= 0) title2ScrollWidth = legacyMetadataWidth;
  if (stationScrollWidth <= 0 && gfx) stationScrollWidth = gfx->width();
  if (title1ScrollWidth <= 0 && gfx) title1ScrollWidth = gfx->width();
  if (title2ScrollWidth <= 0 && gfx) title2ScrollWidth = gfx->width();
  setNeedleSpriteSettings(needleSpriteWidth, needleSpriteHeight, debugBordersEnabled);
}

void saveLastActiveScreen() {
  Preferences prefs;
  prefs.begin("radio", false);
  prefs.putString("activeScreen", getScreenKey(getCurrentScreen()));
  prefs.end();
}

void loadLastActiveScreen() {
  Preferences prefs;
  prefs.begin("radio", true);
  String key = prefs.getString("activeScreen", "");
  prefs.end();
  if (key.isEmpty()) return;

  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);
  for (size_t i = 0; i < count; ++i) {
    if (key == defs[i].key) {
      setCurrentScreen(defs[i].id);
      return;
    }
  }
}
