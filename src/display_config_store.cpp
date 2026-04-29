#include "display_config_store.h"

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>
#include <stdint.h>

#include "config.h"
#include "display_manager.h"
#include "screens/screen_manager.h"
#include "screens/screen_settings.h"
#include "screens/screens.h"

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
extern int stationNameX;
extern int stationNameY;
extern int streamTitle1X;
extern int streamTitle1Y;
extern int streamTitle2X;
extern int streamTitle2Y;
extern int stationScrollWidth;
extern int title1ScrollWidth;
extern int title2ScrollWidth;
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
extern int marqueePauseMs;
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
extern int tftRotation;

extern uint16_t colorStation;
extern uint16_t colorTitle;
extern uint16_t colorBitrate;
extern uint16_t colorVolume;
extern uint16_t colorClock;
extern uint16_t colorNeedleMain;
extern uint16_t colorNeedleRed;
extern uint16_t colorIP;

extern bool redrawClock;
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern bool primaryMarqueeEnabled;
extern bool secondaryMarqueeEnabled;
extern bool quoteMarqueeEnabled;

extern Arduino_GFX* gfx;
extern String quoteApiUrl;

extern void loadBackground();
extern void redrawScreen();
extern void saveSettings();

namespace {
constexpr const char* kDisplayConfigFilename = "/display_config.json";
constexpr const char* kDisplayConfigType = "display-config";
constexpr int kDisplayConfigVersion = 1;

struct VisualSnapshot {
  ScreenId screenId;
  int rotation;
  String background;
  int needlePosLX;
  int needlePosLY;
  int needlePosRX;
  int needlePosRY;
  int needleThickness;
  int needleLenMain;
  int needleLenRed;
  int needleCY;
  int needleSpriteWidth;
  int needleSpriteHeight;
  bool debugBordersEnabled;
  bool leftNeedleMirrored;
  int stationNameX;
  int stationNameY;
  int streamTitle1X;
  int streamTitle1Y;
  int streamTitle2X;
  int streamTitle2Y;
  int stationScrollWidth;
  int title1ScrollWidth;
  int title2ScrollWidth;
  int bitrateX;
  int bitrateY;
  int clockX;
  int clockY;
  int volumeX;
  int volumeY;
  int ipX;
  int ipY;
  int marqueePauseMs;
  bool primaryMarqueeEnabled;
  bool secondaryMarqueeEnabled;
  bool quoteMarqueeEnabled;
  int quoteX;
  int quoteY;
  int quoteW;
  String quoteApiUrl;
  int vuBarsHeight;
  int vuBarsSegments;
  int vuBarsOffsetX;
  int vuBarsOffsetY;
  int vuCardioX;
  int vuCardioY;
  int vuCardioW;
  int vuCardioH;
  int needleMinAngle;
  int needleMaxAngle;
  int needleUpSpeed;
  int needleDownSpeed;
  int needleRotation;
  uint16_t colorStation;
  uint16_t colorTitle;
  uint16_t colorBitrate;
  uint16_t colorVolume;
  uint16_t colorClock;
  uint16_t colorNeedleMain;
  uint16_t colorNeedleRed;
  uint16_t colorIP;
};

VisualSnapshot takeVisualSnapshot() {
  VisualSnapshot snapshot{};
  snapshot.screenId = getCurrentScreen();
  snapshot.rotation = tftRotation;
  snapshot.background = currentBackground;
  snapshot.needlePosLX = needlePosLX;
  snapshot.needlePosLY = needlePosLY;
  snapshot.needlePosRX = needlePosRX;
  snapshot.needlePosRY = needlePosRY;
  snapshot.needleThickness = needleThickness;
  snapshot.needleLenMain = needleLenMain;
  snapshot.needleLenRed = needleLenRed;
  snapshot.needleCY = needleCY;
  snapshot.needleSpriteWidth = needleSpriteWidth;
  snapshot.needleSpriteHeight = needleSpriteHeight;
  snapshot.debugBordersEnabled = debugBordersEnabled;
  snapshot.leftNeedleMirrored = leftNeedleMirrored;
  snapshot.stationNameX = stationNameX;
  snapshot.stationNameY = stationNameY;
  snapshot.streamTitle1X = streamTitle1X;
  snapshot.streamTitle1Y = streamTitle1Y;
  snapshot.streamTitle2X = streamTitle2X;
  snapshot.streamTitle2Y = streamTitle2Y;
  snapshot.stationScrollWidth = stationScrollWidth;
  snapshot.title1ScrollWidth = title1ScrollWidth;
  snapshot.title2ScrollWidth = title2ScrollWidth;
  snapshot.bitrateX = bitrateX;
  snapshot.bitrateY = bitrateY;
  snapshot.clockX = clockX;
  snapshot.clockY = clockY;
  snapshot.volumeX = volumeX;
  snapshot.volumeY = volumeY;
  snapshot.ipX = ipX;
  snapshot.ipY = ipY;
  snapshot.marqueePauseMs = marqueePauseMs;
  snapshot.primaryMarqueeEnabled = primaryMarqueeEnabled;
  snapshot.secondaryMarqueeEnabled = secondaryMarqueeEnabled;
  snapshot.quoteMarqueeEnabled = quoteMarqueeEnabled;
  snapshot.quoteX = quoteX;
  snapshot.quoteY = quoteY;
  snapshot.quoteW = quoteW;
  snapshot.quoteApiUrl = quoteApiUrl;
  snapshot.vuBarsHeight = vuBarsHeight;
  snapshot.vuBarsSegments = vuBarsSegments;
  snapshot.vuBarsOffsetX = vuBarsOffsetX;
  snapshot.vuBarsOffsetY = vuBarsOffsetY;
  snapshot.vuCardioX = vuCardioX;
  snapshot.vuCardioY = vuCardioY;
  snapshot.vuCardioW = vuCardioW;
  snapshot.vuCardioH = vuCardioH;
  snapshot.needleMinAngle = needleMinAngle;
  snapshot.needleMaxAngle = needleMaxAngle;
  snapshot.needleUpSpeed = needleUpSpeed;
  snapshot.needleDownSpeed = needleDownSpeed;
  snapshot.needleRotation = needleRotation;
  snapshot.colorStation = colorStation;
  snapshot.colorTitle = colorTitle;
  snapshot.colorBitrate = colorBitrate;
  snapshot.colorVolume = colorVolume;
  snapshot.colorClock = colorClock;
  snapshot.colorNeedleMain = colorNeedleMain;
  snapshot.colorNeedleRed = colorNeedleRed;
  snapshot.colorIP = colorIP;
  return snapshot;
}

void restoreVisualSnapshot(const VisualSnapshot& snapshot) {
  setCurrentScreen(snapshot.screenId);
  tftRotation = snapshot.rotation;
  currentBackground = snapshot.background;
  needlePosLX = snapshot.needlePosLX;
  needlePosLY = snapshot.needlePosLY;
  needlePosRX = snapshot.needlePosRX;
  needlePosRY = snapshot.needlePosRY;
  needleThickness = snapshot.needleThickness;
  needleLenMain = snapshot.needleLenMain;
  needleLenRed = snapshot.needleLenRed;
  needleCY = snapshot.needleCY;
  needleSpriteWidth = snapshot.needleSpriteWidth;
  needleSpriteHeight = snapshot.needleSpriteHeight;
  debugBordersEnabled = snapshot.debugBordersEnabled;
  leftNeedleMirrored = snapshot.leftNeedleMirrored;
  stationNameX = snapshot.stationNameX;
  stationNameY = snapshot.stationNameY;
  streamTitle1X = snapshot.streamTitle1X;
  streamTitle1Y = snapshot.streamTitle1Y;
  streamTitle2X = snapshot.streamTitle2X;
  streamTitle2Y = snapshot.streamTitle2Y;
  stationScrollWidth = snapshot.stationScrollWidth;
  title1ScrollWidth = snapshot.title1ScrollWidth;
  title2ScrollWidth = snapshot.title2ScrollWidth;
  bitrateX = snapshot.bitrateX;
  bitrateY = snapshot.bitrateY;
  clockX = snapshot.clockX;
  clockY = snapshot.clockY;
  clockSpriteX = clockX;
  clockSpriteY = clockY;
  volumeX = snapshot.volumeX;
  volumeY = snapshot.volumeY;
  ipX = snapshot.ipX;
  ipY = snapshot.ipY;
  marqueePauseMs = snapshot.marqueePauseMs;
  primaryMarqueeEnabled = snapshot.primaryMarqueeEnabled;
  secondaryMarqueeEnabled = snapshot.secondaryMarqueeEnabled;
  quoteMarqueeEnabled = snapshot.quoteMarqueeEnabled;
  quoteX = snapshot.quoteX;
  quoteY = snapshot.quoteY;
  quoteW = snapshot.quoteW;
  quoteApiUrl = snapshot.quoteApiUrl;
  vuBarsHeight = snapshot.vuBarsHeight;
  vuBarsSegments = snapshot.vuBarsSegments;
  vuBarsOffsetX = snapshot.vuBarsOffsetX;
  vuBarsOffsetY = snapshot.vuBarsOffsetY;
  vuCardioX = snapshot.vuCardioX;
  vuCardioY = snapshot.vuCardioY;
  vuCardioW = snapshot.vuCardioW;
  vuCardioH = snapshot.vuCardioH;
  needleMinAngle = snapshot.needleMinAngle;
  needleMaxAngle = snapshot.needleMaxAngle;
  needleUpSpeed = snapshot.needleUpSpeed;
  needleDownSpeed = snapshot.needleDownSpeed;
  needleRotation = snapshot.needleRotation;
  colorStation = snapshot.colorStation;
  colorTitle = snapshot.colorTitle;
  colorBitrate = snapshot.colorBitrate;
  colorVolume = snapshot.colorVolume;
  colorClock = snapshot.colorClock;
  colorNeedleMain = snapshot.colorNeedleMain;
  colorNeedleRed = snapshot.colorNeedleRed;
  colorIP = snapshot.colorIP;
  gfx->setRotation(tftRotation);
  setNeedleSpriteSettings(needleSpriteWidth, needleSpriteHeight, debugBordersEnabled);
}

void serializeCurrentVisualState(JsonObject screenObj) {
  screenObj["background"] = currentBackground;

  JsonObject needle = screenObj["needle"].to<JsonObject>();
  needle["lx"] = needlePosLX;
  needle["ly"] = needlePosLY;
  needle["rx"] = needlePosRX;
  needle["ry"] = needlePosRY;
  needle["thickness"] = needleThickness;
  needle["mainLen"] = needleLenMain;
  needle["redLen"] = needleLenRed;
  needle["centerY"] = needleCY;
  needle["spriteWidth"] = needleSpriteWidth;
  needle["spriteHeight"] = needleSpriteHeight;
  needle["debugBorders"] = debugBordersEnabled;
  needle["leftMirrored"] = leftNeedleMirrored;
  needle["minAngle"] = needleMinAngle;
  needle["maxAngle"] = needleMaxAngle;
  needle["upSpeed"] = needleUpSpeed;
  needle["downSpeed"] = needleDownSpeed;
  needle["rotation"] = needleRotation;

  JsonObject text = screenObj["text"].to<JsonObject>();
  text["stationX"] = stationNameX;
  text["stationY"] = stationNameY;
  text["stationW"] = stationScrollWidth;
  text["title1X"] = streamTitle1X;
  text["title1Y"] = streamTitle1Y;
  text["title1W"] = title1ScrollWidth;
  text["title2X"] = streamTitle2X;
  text["title2Y"] = streamTitle2Y;
  text["title2W"] = title2ScrollWidth;
  text["bitrateX"] = bitrateX;
  text["bitrateY"] = bitrateY;
  text["clockX"] = clockX;
  text["clockY"] = clockY;
  text["volumeX"] = volumeX;
  text["volumeY"] = volumeY;
  text["ipX"] = ipX;
  text["ipY"] = ipY;
  text["marqueePauseMs"] = marqueePauseMs;

  JsonObject marquees = screenObj["marquees"].to<JsonObject>();
  marquees["primaryEnabled"] = primaryMarqueeEnabled;
  marquees["secondaryEnabled"] = secondaryMarqueeEnabled;

  JsonObject quote = screenObj["quote"].to<JsonObject>();
  quote["enabled"] = quoteMarqueeEnabled;
  quote["x"] = quoteX;
  quote["y"] = quoteY;
  quote["w"] = quoteW;
  quote["url"] = quoteApiUrl;

  JsonObject vu = screenObj["vu"].to<JsonObject>();
  vu["barsHeight"] = vuBarsHeight;
  vu["barsSegments"] = vuBarsSegments;
  vu["barsOffsetX"] = vuBarsOffsetX;
  vu["barsOffsetY"] = vuBarsOffsetY;
  vu["cardioX"] = vuCardioX;
  vu["cardioY"] = vuCardioY;
  vu["cardioW"] = vuCardioW;
  vu["cardioH"] = vuCardioH;

  JsonObject colors = screenObj["colors"].to<JsonObject>();
  colors["station"] = colorStation;
  colors["title"] = colorTitle;
  colors["bitrate"] = colorBitrate;
  colors["volume"] = colorVolume;
  colors["clock"] = colorClock;
  colors["ip"] = colorIP;
  colors["needleMain"] = colorNeedleMain;
  colors["needleRed"] = colorNeedleRed;
}

void applyCurrentVisualState(JsonObjectConst screenObj) {
  currentBackground = String(screenObj["background"] | currentBackground);

  JsonObjectConst needle = screenObj["needle"];
  if (!needle.isNull()) {
    needlePosLX = needle["lx"] | needlePosLX;
    needlePosLY = needle["ly"] | needlePosLY;
    needlePosRX = needle["rx"] | needlePosRX;
    needlePosRY = needle["ry"] | needlePosRY;
    needleThickness = needle["thickness"] | needleThickness;
    needleLenMain = needle["mainLen"] | needleLenMain;
    needleLenRed = needle["redLen"] | needleLenRed;
    needleCY = needle["centerY"] | needleCY;
    needleSpriteWidth = needle["spriteWidth"] | needleSpriteWidth;
    needleSpriteHeight = needle["spriteHeight"] | needleSpriteHeight;
    debugBordersEnabled = needle["debugBorders"] | debugBordersEnabled;
    leftNeedleMirrored = needle["leftMirrored"] | leftNeedleMirrored;
    needleMinAngle = needle["minAngle"] | needleMinAngle;
    needleMaxAngle = needle["maxAngle"] | needleMaxAngle;
    needleUpSpeed = needle["upSpeed"] | needleUpSpeed;
    needleDownSpeed = needle["downSpeed"] | needleDownSpeed;
    needleRotation = needle["rotation"] | needleRotation;
  }

  JsonObjectConst text = screenObj["text"];
  if (!text.isNull()) {
    const int legacyMetadataWidth = text["metadataW"] | 0;
    stationNameX = text["stationX"] | stationNameX;
    stationNameY = text["stationY"] | stationNameY;
    stationScrollWidth = text["stationW"] | stationScrollWidth;
    streamTitle1X = text["title1X"] | streamTitle1X;
    streamTitle1Y = text["title1Y"] | streamTitle1Y;
    title1ScrollWidth = text["title1W"] | title1ScrollWidth;
    streamTitle2X = text["title2X"] | streamTitle2X;
    streamTitle2Y = text["title2Y"] | streamTitle2Y;
    title2ScrollWidth = text["title2W"] | title2ScrollWidth;
    bitrateX = text["bitrateX"] | bitrateX;
    bitrateY = text["bitrateY"] | bitrateY;
    clockX = text["clockX"] | clockX;
    clockY = text["clockY"] | clockY;
    clockSpriteX = clockX;
    clockSpriteY = clockY;
    volumeX = text["volumeX"] | volumeX;
    volumeY = text["volumeY"] | volumeY;
    ipX = text["ipX"] | ipX;
    ipY = text["ipY"] | ipY;
    if (stationScrollWidth <= 0) stationScrollWidth = legacyMetadataWidth;
    if (title1ScrollWidth <= 0) title1ScrollWidth = legacyMetadataWidth;
    if (title2ScrollWidth <= 0) title2ScrollWidth = legacyMetadataWidth;
    marqueePauseMs = constrain(text["marqueePauseMs"] | marqueePauseMs, 0, 10000);
  }

  JsonObjectConst marquees = screenObj["marquees"];
  if (!marquees.isNull()) {
    primaryMarqueeEnabled = marquees["primaryEnabled"] | primaryMarqueeEnabled;
    secondaryMarqueeEnabled = marquees["secondaryEnabled"] | secondaryMarqueeEnabled;
  }

  JsonObjectConst quote = screenObj["quote"];
  if (!quote.isNull()) {
    quoteMarqueeEnabled = quote["enabled"] | quoteMarqueeEnabled;
    quoteX = quote["x"] | quoteX;
    quoteY = quote["y"] | quoteY;
    quoteW = quote["w"] | quoteW;
    quoteApiUrl = String(quote["url"] | quoteApiUrl);
  }

  JsonObjectConst vu = screenObj["vu"];
  if (!vu.isNull()) {
    vuBarsHeight = constrain(vu["barsHeight"] | vuBarsHeight, 6, 120);
    vuBarsSegments = constrain(vu["barsSegments"] | vuBarsSegments, 4, 40);
    vuBarsOffsetX = constrain(vu["barsOffsetX"] | vuBarsOffsetX, -200, 200);
    vuBarsOffsetY = constrain(vu["barsOffsetY"] | vuBarsOffsetY, -200, 200);
    vuCardioX = vu["cardioX"] | vuCardioX;
    vuCardioY = vu["cardioY"] | vuCardioY;
    vuCardioW = constrain(vu["cardioW"] | vuCardioW, 80, 480);
    vuCardioH = constrain(vu["cardioH"] | vuCardioH, 40, 320);
  }

  JsonObjectConst colors = screenObj["colors"];
  if (!colors.isNull()) {
    colorStation = colors["station"] | colorStation;
    colorTitle = colors["title"] | colorTitle;
    colorBitrate = colors["bitrate"] | colorBitrate;
    colorVolume = colors["volume"] | colorVolume;
    colorClock = colors["clock"] | colorClock;
    colorIP = colors["ip"] | colorIP;
    colorNeedleMain = colors["needleMain"] | colorNeedleMain;
    colorNeedleRed = colors["needleRed"] | colorNeedleRed;
  }
  setNeedleSpriteSettings(needleSpriteWidth, needleSpriteHeight, debugBordersEnabled);
}

String toHex16(uint16_t value) {
  char buffer[7];
  snprintf(buffer, sizeof(buffer), "0x%04X", value);
  return String(buffer);
}
}

const char* getDefaultDisplayConfigFilename() {
  return kDisplayConfigFilename;
}

bool saveDisplayConfigToFile(const String& filename, String* errorMessage) {
  VisualSnapshot snapshot = takeVisualSnapshot();
  JsonDocument doc;
  doc["type"] = kDisplayConfigType;
  doc["version"] = kDisplayConfigVersion;
  doc["rotation"] = tftRotation;
  doc["activeScreen"] = "vumeter2";

  JsonArray screens = doc["screens"].to<JsonArray>();
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);
  for (size_t i = 0; i < count; ++i) {
    if (!defs[i].implemented) continue;
    setCurrentScreen(defs[i].id);
    loadCurrentScreenSettings();

    JsonObject screenObj = screens.add<JsonObject>();
    screenObj["key"] = defs[i].key;
    screenObj["title"] = defs[i].title;
    serializeCurrentVisualState(screenObj);
  }

  restoreVisualSnapshot(snapshot);

  File file = LittleFS.open(filename, "w", true);
  if (!file) {
    if (errorMessage) *errorMessage = "Failed to open config file for writing";
    return false;
  }

  if (serializeJsonPretty(doc, file) == 0) {
    file.close();
    if (errorMessage) *errorMessage = "Failed to write config file";
    return false;
  }

  file.close();
  return true;
}

bool loadDisplayConfigFromFile(const String& filename, String* errorMessage) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    if (errorMessage) *errorMessage = "Config file not found";
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    if (errorMessage) *errorMessage = String("JSON parse error: ") + err.c_str();
    return false;
  }

  const String type = doc["type"] | "";
  if (type != kDisplayConfigType) {
    if (errorMessage) *errorMessage = "Invalid config type";
    return false;
  }

  VisualSnapshot snapshot = takeVisualSnapshot();
  JsonArrayConst screens = doc["screens"].as<JsonArrayConst>();
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);

  for (JsonObjectConst screenObj : screens) {
    String key = screenObj["key"] | "";
    if (key.isEmpty()) continue;

    for (size_t i = 0; i < count; ++i) {
      if (!defs[i].implemented || key != defs[i].key) continue;
      setCurrentScreen(defs[i].id);
      loadCurrentScreenSettings();
      applyCurrentVisualState(screenObj);
      saveCurrentScreenSettings();
      break;
    }
  }

  int newRotation = doc["rotation"] | snapshot.rotation;
  if (newRotation >= 0 && newRotation <= 3) {
    tftRotation = newRotation;
    gfx->setRotation(tftRotation);
    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putInt("rotation", tftRotation);
    prefs.end();
  }

  String activeScreenKey = doc["activeScreen"] | "";
  ScreenId activeScreen = snapshot.screenId;
  for (size_t i = 0; i < count; ++i) {
    if (!defs[i].implemented) continue;
    if (activeScreenKey == defs[i].key) {
      activeScreen = defs[i].id;
      break;
    }
  }

  setCurrentScreen(activeScreen);
  loadCurrentScreenSettings();
  saveLastActiveScreen();
  loadBackground();
  redrawClock = true;
  redrawScreen();
  saveSettings();
  return true;
}

bool collectDisplayConfigAssetsFromFile(const String& filename, std::vector<String>& assetFilenames, String* errorMessage) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    if (errorMessage) *errorMessage = "Config file not found";
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    if (errorMessage) *errorMessage = String("JSON parse error: ") + err.c_str();
    return false;
  }

  const String type = doc["type"] | "";
  if (type != kDisplayConfigType) {
    if (errorMessage) *errorMessage = "Invalid config type";
    return false;
  }

  JsonArrayConst screens = doc["screens"].as<JsonArrayConst>();
  for (JsonObjectConst screenObj : screens) {
    String background = screenObj["background"] | "";
    if (background.isEmpty()) continue;
    if (!background.startsWith("/")) background = "/" + background;

    bool exists = false;
    for (const String& item : assetFilenames) {
      if (item == background) {
        exists = true;
        break;
      }
    }
    if (!exists) assetFilenames.push_back(background);
  }

  return true;
}

String buildCurrentProfileHeader() {
  String header;
  header.reserve(1600);
  header += "#pragma once\n\n";
  header += "#define DISPLAY_PROFILE_NAME \"Custom Generated\"\n";
  header += "#define DISPLAY_PROFILE_ROTATION " + String(tftRotation) + "\n";
  header += "#define DISPLAY_PROFILE_DEFAULT_BACKGROUND \"" + currentBackground + "\"\n\n";

  header += "#define DISPLAY_PROFILE_NEEDLE_L_X " + String(needlePosLX) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_L_Y " + String(needlePosLY) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_R_X " + String(needlePosRX) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_R_Y " + String(needlePosRY) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_THICKNESS " + String(needleThickness) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_MAIN_LEN " + String(needleLenMain) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_RED_LEN " + String(needleLenRed) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_CENTER_Y " + String(needleCY) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_MIN_ANGLE " + String(needleMinAngle) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_MAX_ANGLE " + String(needleMaxAngle) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_UP_SPEED " + String(needleUpSpeed) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_DOWN_SPEED " + String(needleDownSpeed) + "\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_LEFT_MIRRORED " + String(leftNeedleMirrored ? 1 : 0) + "\n\n";
  header += "#define DISPLAY_PROFILE_NEEDLE_ROTATION " + String(needleRotation) + "\n\n";

  header += "#define DISPLAY_PROFILE_STATION_X " + String(stationNameX) + "\n";
  header += "#define DISPLAY_PROFILE_STATION_Y " + String(stationNameY) + "\n";
  header += "#define DISPLAY_PROFILE_TITLE1_X " + String(streamTitle1X) + "\n";
  header += "#define DISPLAY_PROFILE_TITLE1_Y " + String(streamTitle1Y) + "\n";
  header += "#define DISPLAY_PROFILE_TITLE2_X " + String(streamTitle2X) + "\n";
  header += "#define DISPLAY_PROFILE_TITLE2_Y " + String(streamTitle2Y) + "\n";
  header += "#define DISPLAY_PROFILE_BITRATE_X " + String(bitrateX) + "\n";
  header += "#define DISPLAY_PROFILE_BITRATE_Y " + String(bitrateY) + "\n";
  header += "#define DISPLAY_PROFILE_IP_X " + String(ipX) + "\n";
  header += "#define DISPLAY_PROFILE_IP_Y " + String(ipY) + "\n";
  header += "#define DISPLAY_PROFILE_CLOCK_X " + String(clockX) + "\n";
  header += "#define DISPLAY_PROFILE_CLOCK_Y " + String(clockY) + "\n";
  header += "#define DISPLAY_PROFILE_VOLUME_X " + String(volumeX) + "\n";
  header += "#define DISPLAY_PROFILE_VOLUME_Y " + String(volumeY) + "\n\n";

  header += "#define DISPLAY_PROFILE_COLOR_STATION " + toHex16(colorStation) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_TITLE " + toHex16(colorTitle) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_BITRATE " + toHex16(colorBitrate) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_VOLUME " + toHex16(colorVolume) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_CLOCK " + toHex16(colorClock) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_IP " + toHex16(colorIP) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_NEEDLE_MAIN " + toHex16(colorNeedleMain) + "\n";
  header += "#define DISPLAY_PROFILE_COLOR_NEEDLE_RED " + toHex16(colorNeedleRed) + "\n";
  return header;
}
