#include "web_handlers.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <vector>

#include "config.h"
#include "display_config_store.h"
#include "led_music.h"
#include "screens/screen_manager.h"
#include "screens/screen_settings.h"
#include "screens/screens.h"
#include "sleep_timer.h"
#include "web_interface.h"
#include "web_interface_thems.h"
#include "backlight_control.h"
#include "web_ota.h"
#include "wifi_setup.h"
#include "yoEncoder.h"

extern WebServer server;
extern Audio audio;
extern SleepTimer sleepTimer;
extern yoEncoder* encoder;
extern SemaphoreHandle_t xMutex;

extern String currentStreamName;
extern String currentPlaylistFile;
extern String currentPlayingPlaylistFile;
extern String currentBackground;
extern String currentStreamURL;
extern String icy_station;
extern String icy_streamtitle;
extern String icy_bitrate;
extern String audio_codec;
extern String audio_frequency;
extern String audio_bitness;
extern int stationTextFontSize;

extern int currentVolume;
extern int currentStreamIndex;
extern int streamCount;
extern int needlePosLX, needlePosLY, needlePosRX, needlePosRY;
extern int eqBalance, eqTreble, eqMid, eqBass;

extern Arduino_GFX *gfx;
extern int tftRotation;
extern int needleThickness, needleLenMain, needleLenRed, needleCY;
extern int needleSpriteWidth, needleSpriteHeight;
extern int stationNameX, stationNameY;
extern int streamTitle1X, streamTitle1Y;
extern int streamTitle2X, streamTitle2Y;
extern int stationScrollWidth;
extern int title1ScrollWidth;
extern int title2ScrollWidth;
extern int marqueePauseMs;
extern int bitrateX, bitrateY;
extern int clockX, clockY, clockSpriteX, clockSpriteY;
extern int volumeX, volumeY;
extern int ipX, ipY;
extern int quoteX, quoteY, quoteW;
extern String quoteApiUrl;
extern int vuBarsHeight, vuBarsSegments, vuBarsOffsetX, vuBarsOffsetY;
extern int vuCardioX, vuCardioY, vuCardioW, vuCardioH;
extern int needleMinAngle, needleMaxAngle;
extern int needleUpSpeed, needleDownSpeed;
extern int needleRotation;
extern uint16_t colorStation, colorTitle, colorBitrate, colorVolume, colorClock, colorNeedleMain, colorNeedleRed, colorIP;

extern bool radioStarted;
extern bool isPaused;
extern bool pendingToggle;
extern bool redrawClock;
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern bool ledMusicEnabled;
extern bool vuMeterEnabled;
extern bool primaryMarqueeEnabled;
extern bool secondaryMarqueeEnabled;
extern bool quoteMarqueeEnabled;
extern int ledStripCount;
extern int ledEffectIndex;
#ifdef WS2812B_PIN
extern LedMusic ledMusic;
#endif

extern void nextStream();
extern void prevStream();
extern void selectStream(int idx);
extern void loadPlaylistMetadata(String filename);
extern void loadBackground();
extern void redrawScreen();
extern void redrawScreenLocked();
extern void refreshVuOnly();
extern void setDisplayTaskPaused(bool paused);
extern void drawVolumeDisplay();
extern void saveSettings();
extern void scheduleDisplaySettingsSave();
extern void flushPendingDisplaySettingsSave();
extern void applyAudioEq();
extern bool setNeedleSpriteSettings(int width, int height, bool debugBorders);
extern bool getStreamByIndex(int idx, String& outName, String& outURL);

namespace {
constexpr const char* kMyPlaylistName = "/favorites.csv";
constexpr const char* kDisplayPackageTempConfig = "/display_package_export.json";
constexpr const char* kDisplayPackageTempTar = "/display_package.tar";
constexpr const char* kDisplayPackageEntryConfig = "display_config.json";
constexpr size_t kTarBlockSize = 512;

bool g_uploadInProgress = false;
bool g_pendingTarImport = false;
bool g_uploadHadError = false;
String g_pendingTarPath;
String g_uploadResponseMessage;

String normalizeFsPath(String path) {
  if (!path.startsWith("/")) path = "/" + path;
  return path;
}

String getBaseName(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

bool isImageAssetFilename(const String& filename) {
  return filename.endsWith(".jpg") || filename.endsWith(".jpeg") || filename.endsWith(".png");
}

bool isSafeTarEntryName(const String& name) {
  if (name.isEmpty()) return false;
  if (name.startsWith("/") || name.indexOf("..") >= 0 || name.indexOf('\\') >= 0) return false;
  return name.indexOf(':') < 0;
}

size_t tarPaddedSize(size_t size) {
  return ((size + kTarBlockSize - 1) / kTarBlockSize) * kTarBlockSize;
}

void fillTarOctal(char* dest, size_t len, size_t value) {
  if (len == 0) return;
  memset(dest, 0, len);
  snprintf(dest, len, "%0*o", static_cast<int>(len - 1), static_cast<unsigned int>(value));
}

bool appendTarEntry(File& tarFile, File& file, const String& entryName) {
  if (!file || entryName.isEmpty() || entryName.length() >= 100) return false;

  uint8_t header[kTarBlockSize];
  memset(header, 0, sizeof(header));

  memcpy(header, entryName.c_str(), entryName.length());
  memcpy(header + 100, "0000777", 7);
  memcpy(header + 108, "0000000", 7);
  memcpy(header + 116, "0000000", 7);
  fillTarOctal(reinterpret_cast<char*>(header + 124), 12, static_cast<size_t>(file.size()));
  fillTarOctal(reinterpret_cast<char*>(header + 136), 12, 0);
  memset(header + 148, ' ', 8);
  header[156] = '0';
  memcpy(header + 257, "ustar", 5);
  memcpy(header + 263, "00", 2);

  unsigned int checksum = 0;
  for (size_t i = 0; i < sizeof(header); ++i) checksum += header[i];
  snprintf(reinterpret_cast<char*>(header + 148), 8, "%06o", checksum);
  header[154] = '\0';
  header[155] = ' ';

  if (tarFile.write(header, sizeof(header)) != sizeof(header)) return false;

  uint8_t buffer[1024];
  size_t remaining = file.size();
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
    size_t got = file.read(buffer, chunk);
    if (got == 0) return false;
    if (tarFile.write(buffer, got) != got) return false;
    remaining -= got;
  }

  const size_t padding = tarPaddedSize(static_cast<size_t>(file.size())) - static_cast<size_t>(file.size());
  if (padding > 0) {
    uint8_t zeros[kTarBlockSize] = {0};
    if (tarFile.write(zeros, padding) != padding) return false;
  }

  return true;
}

bool buildDisplayPackageTar(const String& tarPath, String* errorMessage) {
  LittleFS.remove(tarPath);

  if (!saveDisplayConfigToFile(kDisplayPackageTempConfig, errorMessage)) {
    return false;
  }

  std::vector<String> assets;
  if (!collectDisplayConfigAssetsFromFile(kDisplayPackageTempConfig, assets, errorMessage)) {
    LittleFS.remove(kDisplayPackageTempConfig);
    return false;
  }

  File tarFile = LittleFS.open(tarPath, "w", true);
  if (!tarFile) {
    LittleFS.remove(kDisplayPackageTempConfig);
    if (errorMessage) *errorMessage = "Failed to create package file";
    return false;
  }

  File configFile = LittleFS.open(kDisplayPackageTempConfig, "r");
  if (!configFile) {
    tarFile.close();
    LittleFS.remove(tarPath);
    LittleFS.remove(kDisplayPackageTempConfig);
    if (errorMessage) *errorMessage = "Failed to open temporary config";
    return false;
  }

  bool ok = appendTarEntry(tarFile, configFile, kDisplayPackageEntryConfig);
  configFile.close();

  for (size_t i = 0; ok && i < assets.size(); ++i) {
    String path = normalizeFsPath(assets[i]);
    if (!LittleFS.exists(path)) continue;
    File assetFile = LittleFS.open(path, "r");
    if (!assetFile) continue;
    ok = appendTarEntry(tarFile, assetFile, getBaseName(path));
    assetFile.close();
  }

  if (ok) {
    uint8_t zeros[kTarBlockSize] = {0};
    ok = tarFile.write(zeros, sizeof(zeros)) == sizeof(zeros)
      && tarFile.write(zeros, sizeof(zeros)) == sizeof(zeros);
  }

  tarFile.close();
  LittleFS.remove(kDisplayPackageTempConfig);

  if (!ok) {
    LittleFS.remove(tarPath);
    if (errorMessage) *errorMessage = "Failed to build package";
    return false;
  }

  return true;
}

bool skipFileBytes(File& file, size_t bytesToSkip) {
  uint8_t buffer[256];
  while (bytesToSkip > 0) {
    size_t chunk = bytesToSkip > sizeof(buffer) ? sizeof(buffer) : bytesToSkip;
    size_t got = file.read(buffer, chunk);
    if (got == 0) return false;
    bytesToSkip -= got;
  }
  return true;
}

bool extractDisplayPackageTar(const String& tarPath, String* errorMessage) {
  File tar = LittleFS.open(tarPath, "r");
  if (!tar) {
    if (errorMessage) *errorMessage = "Failed to open package";
    return false;
  }

  String importedConfigPath;
  uint8_t header[kTarBlockSize];
  while (true) {
    if (tar.read(header, sizeof(header)) != sizeof(header)) {
      tar.close();
      if (errorMessage) *errorMessage = "Invalid TAR header";
      return false;
    }

    bool allZero = true;
    for (size_t i = 0; i < sizeof(header); ++i) {
      if (header[i] != 0) {
        allZero = false;
        break;
      }
    }
    if (allZero) break;

    char nameBuf[101];
    memcpy(nameBuf, header, 100);
    nameBuf[100] = '\0';
    String entryName(nameBuf);
    entryName.trim();

    char sizeBuf[13];
    memcpy(sizeBuf, header + 124, 12);
    sizeBuf[12] = '\0';
    size_t entrySize = strtoul(sizeBuf, nullptr, 8);
    char typeFlag = header[156];

    const size_t alignedSize = tarPaddedSize(entrySize);
    if (!isSafeTarEntryName(entryName)) {
      if (!skipFileBytes(tar, alignedSize)) {
        tar.close();
        if (errorMessage) *errorMessage = "Failed to skip invalid TAR entry";
        return false;
      }
      continue;
    }

    const bool regularFile = (typeFlag == 0 || typeFlag == '0');
    String targetName = normalizeFsPath(getBaseName(entryName));
    const bool allowedFile = targetName.endsWith(".json") || isImageAssetFilename(targetName);
    if (!regularFile || !allowedFile) {
      if (!skipFileBytes(tar, alignedSize)) {
        tar.close();
        if (errorMessage) *errorMessage = "Failed to skip unsupported TAR entry";
        return false;
      }
      continue;
    }

    File out = LittleFS.open(targetName, "w", true);
    if (!out) {
      tar.close();
      if (errorMessage) *errorMessage = "Failed to extract file: " + targetName;
      return false;
    }

    size_t remaining = entrySize;
    uint8_t buffer[1024];
    while (remaining > 0) {
      size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
      size_t got = tar.read(buffer, chunk);
      if (got == 0) {
        out.close();
        tar.close();
        if (errorMessage) *errorMessage = "Unexpected end of TAR file";
        return false;
      }
      if (out.write(buffer, got) != got) {
        out.close();
        tar.close();
        if (errorMessage) *errorMessage = "Failed to write extracted file";
        return false;
      }
      remaining -= got;
      delay(0);
    }
    out.close();

    const size_t padding = alignedSize - entrySize;
    if (padding > 0 && !skipFileBytes(tar, padding)) {
      tar.close();
      if (errorMessage) *errorMessage = "Failed to skip TAR padding";
      return false;
    }

    if (targetName.endsWith(".json") && importedConfigPath.isEmpty()) importedConfigPath = targetName;
  }

  tar.close();

  if (importedConfigPath.isEmpty()) {
    if (errorMessage) *errorMessage = "Package does not contain display config";
    return false;
  }

  return loadDisplayConfigFromFile(importedConfigPath, errorMessage);
}

String sanitizePlaylistField(String value) {
  value.replace("\n", " ");
  value.replace("\r", " ");
  value.replace("\t", " ");
  value.trim();
  return value;
}

bool ensureMyPlaylistExists(String* errorMessage = nullptr) {
  if (LittleFS.exists(kMyPlaylistName)) return true;
  File file = LittleFS.open(kMyPlaylistName, "w", true);
  if (!file) {
    if (errorMessage) *errorMessage = "Failed to create favorites.csv";
    return false;
  }
  file.close();
  return true;
}

bool appendStationToMyPlaylist(const String& name, const String& url, String* errorMessage = nullptr) {
  File file = LittleFS.open(kMyPlaylistName, "a", true);
  if (!file) {
    if (errorMessage) *errorMessage = "Failed to open favorites.csv";
    return false;
  }

  const String safeName = sanitizePlaylistField(name);
  const String safeUrl = sanitizePlaylistField(url);
  file.printf("%s\t%s\t0\n", safeName.c_str(), safeUrl.c_str());
  file.close();
  return true;
}
}

bool isWebFileTransferActive() {
  return g_uploadInProgress || g_pendingTarImport;
}

void serviceWebBackgroundTasks() {
  if (!g_pendingTarImport || g_pendingTarPath.isEmpty()) return;

  const String tarPath = g_pendingTarPath;
  g_pendingTarImport = false;
  g_pendingTarPath = "";

  String errorMessage;
  if (!extractDisplayPackageTar(tarPath, &errorMessage)) {
    Serial.printf("Display package import failed: %s\n", errorMessage.c_str());
  }
  LittleFS.remove(tarPath);
}

static void handleDeleteBg() {
  if (server.hasArg("bg")) {
    String filename = server.arg("bg");
    if (!filename.startsWith("/")) filename = "/" + filename;
    if (LittleFS.exists(filename) && (filename.endsWith(".jpg") || filename.endsWith(".jpeg"))) {
      LittleFS.remove(filename);
      //Serial.printf("Deleted background: %s\n", filename.c_str());
      if (currentBackground == filename) {
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
          String name = String(file.name());
          if (name.endsWith(".jpg") || name.endsWith(".jpeg")) {
            currentBackground = name;
            loadBackground();
            redrawScreen();
            saveSettings();
            break;
          }
          file = root.openNextFile();
        }
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleDeletePlaylist() {
  if (server.hasArg("pl")) {
    String filename = server.arg("pl");
    if (!filename.startsWith("/")) filename = "/" + filename;
    if (LittleFS.exists(filename) && filename.endsWith(".csv")) {
      LittleFS.remove(filename);
      //Serial.printf("Deleted playlist: %s\n", filename.c_str());
      if (currentPlaylistFile == filename) {
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
          String name = String(file.name());
          if (name.endsWith(".csv")) {
            currentPlaylistFile = name;
            loadPlaylistMetadata(name);
            if (streamCount > 0) selectStream(0);
            break;
          }
          file = root.openNextFile();
        }
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleVisualReset() {
  Preferences prefsLocal;
  prefsLocal.begin("radio", false);
  const char* keys[] = {
    "stationX","stationY","title1X","title1Y","title2X","title2Y","bitrateX","bitrateY","clockY","volumeX","volumeY",
    "colorStation","colorTitle","colorBitrate","colorVolume","colorClock","colorNeedleMain","colorNeedleRed",
    "needleLX","needleLY","needleRX","needleRY","needleThk","needleMain","needleRed","needleCY","needleDbg","needleSW","needleSH",
    "needleMirrorL","secMq",
    "needleMinAngle","needleMaxAngle","needleUpSpeed","needleDownSpeed","needleRotation"
  };
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) prefsLocal.remove(keys[i]);
  prefsLocal.end();
  server.send(200, "text/plain", "Visual settings reset. Rebooting...");
  delay(300);
  ESP.restart();
}

static void handleRoot() {
  if (WiFi.getMode() == WIFI_AP) server.send(200, "text/html", getWiFiSetupHTML());
  else server.send(200, "text/html", getWebInterfaceHTML());
}

static void handleThemsCss() {
  const char* kPath = "/thems.css";
  if (LittleFS.exists(kPath)) {
    File file = LittleFS.open(kPath, "r", true);
    if (file) {
      server.sendHeader("Cache-Control", "max-age=300");
      server.streamFile(file, "text/css");
      file.close();
      return;
    }
  }

  server.sendHeader("Cache-Control", "max-age=300");
  server.send_P(200, "text/css", getThemsCssDefault());
}

static void handleSetBrightness() {
  if (server.hasArg("b")) {
    int b = server.arg("b").toInt();
    b = constrain(b, 0, 255);
    setTftBrightness(uint8_t(b), false);
    scheduleDisplaySettingsSave();
    server.send(200, "text/plain", String(b));
    return;
  }
  server.send(400, "text/plain", "Missing b");
}

static void handleAPIBrightness() {
  server.send(200, "application/json", String("{\"brightness\":") + String(getTftBrightness()) + "}");
}

namespace {
String urlDecodePath(const String& input) {
  String out;
  out.reserve(input.length());
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '%' && i + 2 < input.length()) {
      char h1 = input[i + 1];
      char h2 = input[i + 2];
      auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        return -1;
      };
      int v1 = hex(h1);
      int v2 = hex(h2);
      if (v1 >= 0 && v2 >= 0) {
        out += char((v1 << 4) | v2);
        i += 2;
        continue;
      }
    }
    if (c == '+') c = ' ';
    out += c;
  }
  return out;
}

const char* contentTypeForPath(const String& path) {
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".csv")) return "text/csv";
  if (path.endsWith(".txt")) return "text/plain";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "application/octet-stream";
}

bool tryServeStaticFromLittleFS() {
  String path = urlDecodePath(server.uri());
  if (!path.startsWith("/")) path = "/" + path;
  if (path == "/") return false;
  if (path.indexOf("..") >= 0) return false;

  if (!LittleFS.exists(path)) return false;

  File file = LittleFS.open(path, "r", true);
  if (!file) return false;

  server.sendHeader("Cache-Control", "max-age=300");
  server.streamFile(file, contentTypeForPath(path));
  file.close();
  return true;
}
}

static void handleNotFound() {
  if (tryServeStaticFromLittleFS()) return;
  server.send(404, "text/plain", "404 - Not Found");
}
static void handleNext() { nextStream(); server.send(200, "text/plain", "OK"); }
static void handlePrev() { prevStream(); server.send(200, "text/plain", "OK"); }

static void handlePlay() {
  pendingToggle = true;
  //Serial.println("Requested: toggle play/pause");
  server.send(200, "text/plain", "OK");
}

static void handleSetLed() {
  if (server.hasArg("enabled")) {
    ledMusicEnabled = server.arg("enabled").toInt() != 0;
    scheduleDisplaySettingsSave();
#ifdef WS2812B_PIN
    if (!ledMusicEnabled) {
      ledMusic.off();
    }
#endif
  }
  server.send(200, "text/plain", ledMusicEnabled ? "1" : "0");
}

static void handleSetVuMeter() {
  if (server.hasArg("enabled")) {
    vuMeterEnabled = server.arg("enabled").toInt() != 0;
    scheduleDisplaySettingsSave();
    redrawScreen();
  }
  server.send(200, "text/plain", vuMeterEnabled ? "1" : "0");
}

static void handleSetVuBars() {
  if (server.hasArg("h")) vuBarsHeight = constrain(server.arg("h").toInt(), 6, 120);
  if (server.hasArg("seg")) vuBarsSegments = constrain(server.arg("seg").toInt(), 4, 40);
  if (server.hasArg("x")) vuBarsOffsetX = constrain(server.arg("x").toInt(), -200, 200);
  if (server.hasArg("y")) vuBarsOffsetY = constrain(server.arg("y").toInt(), -200, 200);
  scheduleDisplaySettingsSave();
  redrawScreen();
  server.send(200, "text/plain", "OK");
}

static void handleSetVuCardio() {
  const int maxW = gfx ? gfx->width() : 480;
  const int maxH = gfx ? gfx->height() : 320;
  if (server.hasArg("x")) vuCardioX = constrain(server.arg("x").toInt(), 0, maxW);
  if (server.hasArg("y")) vuCardioY = constrain(server.arg("y").toInt(), 0, maxH);
  if (server.hasArg("w")) vuCardioW = constrain(server.arg("w").toInt(), 80, maxW);
  if (server.hasArg("h")) vuCardioH = constrain(server.arg("h").toInt(), 40, maxH);
  scheduleDisplaySettingsSave();
  refreshVuOnly();
  redrawScreen();
  server.send(200, "text/plain", "OK");
}

static void handleSetMarquee1() {
  if (server.hasArg("enabled")) {
    primaryMarqueeEnabled = server.arg("enabled").toInt() != 0;
  }
  if (server.hasArg("pause")) {
    marqueePauseMs = constrain(server.arg("pause").toInt(), 0, 10000);
  }
  if (server.hasArg("enabled") || server.hasArg("pause")) {
    scheduleDisplaySettingsSave();
    redrawScreen();
  }
  server.send(200, "text/plain", primaryMarqueeEnabled ? "1" : "0");
}

static void handleSetMarquee2() {
  if (server.hasArg("enabled")) {
    secondaryMarqueeEnabled = server.arg("enabled").toInt() != 0;
  }
  if (server.hasArg("pause")) {
    marqueePauseMs = constrain(server.arg("pause").toInt(), 0, 10000);
  }
  if (server.hasArg("enabled") || server.hasArg("pause")) {
    scheduleDisplaySettingsSave();
    redrawScreen();
  }
  server.send(200, "text/plain", secondaryMarqueeEnabled ? "1" : "0");
}

static void handleSetQuote() {
  if (server.hasArg("enabled")) {
    quoteMarqueeEnabled = server.arg("enabled").toInt() != 0;
  }
  if (server.hasArg("url")) {
    String url = server.arg("url");
    url.trim();
    if (url.length() > 0) {
      quoteApiUrl = url;
    }
  }
  if (server.hasArg("pause")) {
    marqueePauseMs = constrain(server.arg("pause").toInt(), 0, 10000);
  }
  if (server.hasArg("enabled") || server.hasArg("url") || server.hasArg("pause")) {
    scheduleDisplaySettingsSave();
    redrawScreen();
  }
  server.send(200, "text/plain", quoteMarqueeEnabled ? "1" : "0");
}

static void handleAPILed() {
  String json = "{";
  json += "\"enabled\":";
  json += ledMusicEnabled ? "true" : "false";
  json += ",\"count\":" + String(ledStripCount);
  json += ",\"effect\":" + String(ledEffectIndex);
#ifdef WS2812B_PIN
  json += ",\"effectName\":\"" + String(ledMusic.getEffectName()) + "\"";
  json += ",\"effectCount\":" + String(ledMusic.getEffectCount());
  json += ",\"effects\":[";
  for (uint8_t i = 0; i < ledMusic.getEffectCount(); ++i) {
    if (i) json += ",";
    const auto& effect = ledMusic.getEffectDefinition(i);
    json += "{\"index\":" + String(i) + ",\"key\":\"" + String(effect.key) + "\",\"name\":\"" + String(effect.title) + "\"}";
  }
  json += "]";
#else
  json += ",\"effectName\":\"Unavailable\"";
  json += ",\"effectCount\":0,\"effects\":[]";
#endif
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAPIVuMeter() {
  String json = "{";
  json += "\"enabled\":";
  json += vuMeterEnabled ? "true" : "false";
  json += ",\"height\":" + String(vuBarsHeight);
  json += ",\"segments\":" + String(vuBarsSegments);
  json += ",\"x\":" + String(vuBarsOffsetX);
  json += ",\"y\":" + String(vuBarsOffsetY);
  json += ",\"cardioX\":" + String(vuCardioX);
  json += ",\"cardioY\":" + String(vuCardioY);
  json += ",\"cardioW\":" + String(vuCardioW);
  json += ",\"cardioH\":" + String(vuCardioH);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAPIMarquee1() {
  String json = "{";
  json += "\"enabled\":";
  json += primaryMarqueeEnabled ? "true" : "false";
  json += ",\"pauseMs\":" + String(marqueePauseMs);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAPIMarquee2() {
  String json = "{";
  json += "\"enabled\":";
  json += secondaryMarqueeEnabled ? "true" : "false";
  json += ",\"pauseMs\":" + String(marqueePauseMs);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAPIStationFont() {
  String json = "{";
  json += "\"sizeIndex\":" + String(stationTextFontSize) + ",";
  json += "\"sizeKey\":\"";
  if (stationTextFontSize <= 0) json += "small";
  else if (stationTextFontSize >= 2) json += "large";
  else json += "normal";
  json += "\",\"label\":\"";
  if (stationTextFontSize <= 0) json += "Small";
  else if (stationTextFontSize >= 2) json += "Large";
  else json += "Normal";
  json += "\"}";
  server.send(200, "application/json", json);
}

static void handleAPIQuote() {
  String json = "{";
  json += "\"enabled\":";
  json += quoteMarqueeEnabled ? "true" : "false";
  json += ",\"x\":" + String(quoteX);
  json += ",\"y\":" + String(quoteY);
  json += ",\"w\":" + String(quoteW);
  json += ",\"pauseMs\":" + String(marqueePauseMs);
  json += ",\"url\":\"" + quoteApiUrl + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

static void handleSetLedConfig() {
#ifdef WS2812B_PIN
  if (server.hasArg("count")) {
    int requestedCount = constrain(server.arg("count").toInt(), 1, WS2812B_MAX_LED_COUNT);
    if (ledMusic.setLedCount(requestedCount)) {
      ledStripCount = ledMusic.getLedCount();
      ledMusic.setEffectIndex(ledEffectIndex);
    }
  }

  if (server.hasArg("effect")) {
    ledEffectIndex = constrain(server.arg("effect").toInt(), 0, ledMusic.getEffectCount() - 1);
    ledMusic.setEffectIndex(ledEffectIndex);
  } else if (server.hasArg("dir")) {
    String dir = server.arg("dir");
    if (dir == "next") ledMusic.nextEffect();
    else if (dir == "prev") ledMusic.prevEffect();
    ledEffectIndex = ledMusic.getEffectIndex();
  }
#endif

  scheduleDisplaySettingsSave();
  handleAPILed();
}

static void handlePlayURL() {
  if (server.hasArg("url")) {
    String url = server.arg("url");
    if (url.startsWith("http://") || url.startsWith("https://")) {
      audio.connecttohost(url.c_str());
      audio.setVolume(currentVolume);
      radioStarted = true;
      isPaused = false;
      currentStreamName = "Direct URL";
      currentStreamURL = url;
      redrawScreen();
      //Serial.printf("Playing URL: %s\n", url.c_str());
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleSetVolume() {
  if (server.hasArg("v")) {
    currentVolume = constrain(server.arg("v").toInt(), 0, 50);
    audio.setVolume(currentVolume);
    if (encoder) encoder->setEncoderValue(currentVolume);
    drawVolumeDisplay();
    //Serial.printf("Volume set to %d\n", currentVolume);
  }
  server.send(200, "text/plain", "OK");
}

static void handleSetEq() {
  bool changed = false;
  if (server.hasArg("balance")) {
    eqBalance = constrain(server.arg("balance").toInt(), -16, 16);
    changed = true;
  }
  if (server.hasArg("treble")) {
    eqTreble = constrain(server.arg("treble").toInt(), -12, 12);
    changed = true;
  }
  if (server.hasArg("mid")) {
    eqMid = constrain(server.arg("mid").toInt(), -12, 12);
    changed = true;
  }
  if (server.hasArg("bass")) {
    eqBass = constrain(server.arg("bass").toInt(), -12, 12);
    changed = true;
  }

  if (changed) {
    applyAudioEq();
    scheduleDisplaySettingsSave();
  }
  server.send(200, "text/plain", "OK");
}

static void handleSetNeedle() {
  if (server.hasArg("lx")) needlePosLX = server.arg("lx").toInt();
  if (server.hasArg("ly")) needlePosLY = server.arg("ly").toInt();
  if (server.hasArg("rx")) needlePosRX = server.arg("rx").toInt();
  if (server.hasArg("ry")) needlePosRY = server.arg("ry").toInt();
  server.send(200, "text/plain", "OK");
}

static void handleSetNeedleAppearance() {
  if (server.hasArg("thk")) needleThickness = server.arg("thk").toInt();
  if (server.hasArg("main")) needleLenMain = server.arg("main").toInt();
  if (server.hasArg("red")) needleLenRed = server.arg("red").toInt();
  if (server.hasArg("cy")) needleCY = server.arg("cy").toInt();

  int newSpriteW = needleSpriteWidth;
  int newSpriteH = needleSpriteHeight;
  bool newDebugBorders = debugBordersEnabled;
  bool newLeftMirrored = leftNeedleMirrored;
  int newNeedleRotation = needleRotation;
  bool spriteOrDebugChanged = false;

  if (server.hasArg("sw")) {
    newSpriteW = constrain(server.arg("sw").toInt(), 16, 480);
    spriteOrDebugChanged = true;
  }
  if (server.hasArg("sh")) {
    newSpriteH = constrain(server.arg("sh").toInt(), 16, 480);
    spriteOrDebugChanged = true;
  }
  if (server.hasArg("dbg")) {
    newDebugBorders = server.arg("dbg").toInt() != 0;
    spriteOrDebugChanged = true;
  }
  if (server.hasArg("mirrorL")) {
    newLeftMirrored = server.arg("mirrorL").toInt() != 0;
  }
  if (server.hasArg("rot")) {
    int value = server.arg("rot").toInt();
    value = ((value % 360) + 360) % 360;
    newNeedleRotation = (value / 90) * 90;
  }

  if (spriteOrDebugChanged) {
    if (!setNeedleSpriteSettings(newSpriteW, newSpriteH, newDebugBorders)) {
      server.send(500, "text/plain", "Failed to apply needle sprite settings");
      return;
    }
  }

  leftNeedleMirrored = newLeftMirrored;
  needleRotation = newNeedleRotation;
  refreshVuOnly();
  server.send(200, "text/plain", "OK");
}

static void handleSetTextPos() {
  if (server.hasArg("stationX")) stationNameX = server.arg("stationX").toInt();
  if (server.hasArg("stationY")) stationNameY = server.arg("stationY").toInt();
  if (server.hasArg("title1X")) streamTitle1X = server.arg("title1X").toInt();
  if (server.hasArg("title1Y")) streamTitle1Y = server.arg("title1Y").toInt();
  if (server.hasArg("title2X")) streamTitle2X = server.arg("title2X").toInt();
  if (server.hasArg("title2Y")) streamTitle2Y = server.arg("title2Y").toInt();
  if (server.hasArg("stationW")) stationScrollWidth = constrain(server.arg("stationW").toInt(), 40, gfx->width());
  if (server.hasArg("title1W")) title1ScrollWidth = constrain(server.arg("title1W").toInt(), 40, gfx->width());
  if (server.hasArg("title2W")) title2ScrollWidth = constrain(server.arg("title2W").toInt(), 40, gfx->width());
  if (server.hasArg("marqueePauseMs")) marqueePauseMs = constrain(server.arg("marqueePauseMs").toInt(), 0, 10000);
  if (server.hasArg("bitrateX")) bitrateX = server.arg("bitrateX").toInt();
  if (server.hasArg("bitrateY")) bitrateY = server.arg("bitrateY").toInt();
  if (server.hasArg("clockX")) { clockX = server.arg("clockX").toInt(); clockSpriteX = clockX; redrawClock = true; }
  if (server.hasArg("clockY")) { clockY = server.arg("clockY").toInt(); clockSpriteY = clockY; redrawClock = true; }
  if (server.hasArg("volumeX")) volumeX = server.arg("volumeX").toInt();
  if (server.hasArg("volumeY")) volumeY = server.arg("volumeY").toInt();
  if (server.hasArg("ipX")) ipX = server.arg("ipX").toInt();
  if (server.hasArg("ipY")) ipY = server.arg("ipY").toInt();
  if (server.hasArg("quoteX")) quoteX = server.arg("quoteX").toInt();
  if (server.hasArg("quoteY")) quoteY = server.arg("quoteY").toInt();
  if (server.hasArg("quoteW")) quoteW = server.arg("quoteW").toInt();
  scheduleDisplaySettingsSave();
  redrawScreen();
  server.send(200, "text/plain", "OK");
}

static void handleSetStationFont() {
  if (server.hasArg("size")) {
    String sizeArg = server.arg("size");
    if (sizeArg == "small") stationTextFontSize = 0;
    else if (sizeArg == "large") stationTextFontSize = 2;
    else if (sizeArg == "normal") stationTextFontSize = 1;
    else stationTextFontSize = constrain(sizeArg.toInt(), 0, 2);
    scheduleDisplaySettingsSave();
    redrawScreen();
  }
  handleAPIStationFont();
}

static void handleSelectStream() { if (server.hasArg("idx")) selectStream(server.arg("idx").toInt()); server.send(200, "text/plain", "OK"); }

static void handleSelectPlaylist() {
  if (server.hasArg("pl")) {
    String filename = server.arg("pl");
    loadPlaylistMetadata(filename);
    saveSettings();
  }
  server.send(200, "text/plain", "OK");
}

static void handleMyPlaylist() {
  String errorMessage;
  if (!ensureMyPlaylistExists(&errorMessage)) {
    server.send(500, "text/plain", errorMessage);
    return;
  }
  server.send(200, "text/plain", kMyPlaylistName);
}

static void handleAddToMyPlaylist() {
  if (!server.hasArg("idx")) {
    server.send(400, "text/plain", "Missing stream index");
    return;
  }

  String streamName;
  String streamUrl;
  const int idx = server.arg("idx").toInt();
  if (!getStreamByIndex(idx, streamName, streamUrl)) {
    server.send(404, "text/plain", "Stream not found");
    return;
  }

  String errorMessage;
  if (!ensureMyPlaylistExists(&errorMessage)) {
    server.send(500, "text/plain", errorMessage);
    return;
  }

  if (!appendStationToMyPlaylist(streamName, streamUrl, &errorMessage)) {
    server.send(500, "text/plain", errorMessage);
    return;
  }

  server.send(200, "text/plain", "Added to favorites.csv");
}

static void handleSelectBg() {
  if (server.hasArg("bg")) {
    String newBg = server.arg("bg");
    if (!newBg.startsWith("/")) newBg = "/" + newBg;
    if (LittleFS.exists(newBg)) {
      currentBackground = newBg;
      loadBackground();
      redrawClock = true;
      saveSettings();
      redrawScreen();
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleAPIPlayer() { server.send(200, "application/json", getPlayerStatus()); }

static void handleSelectScreen() {
  if (!server.hasArg("screen")) { server.send(400, "text/plain", "Missing screen parameter"); return; }
  flushPendingDisplaySettingsSave();
  String key = server.arg("screen");
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);
  for (size_t i = 0; i < count; ++i) {
    if (key == defs[i].key) {
      if (!defs[i].implemented) { server.send(400, "text/plain", "Screen is not implemented yet"); return; }
      saveCurrentScreenSettings();
      setDisplayTaskPaused(true);
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        setDisplayTaskPaused(false);
        server.send(503, "text/plain", "Display busy");
        return;
      }
      setCurrentScreen(defs[i].id);
      loadCurrentScreenSettings();
      loadBackground();
      saveLastActiveScreen();
      redrawScreenLocked();
      xSemaphoreGive(xMutex);
      setDisplayTaskPaused(false);
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(404, "text/plain", "Unknown screen");
}

static void handleAPIScreens() {
  String json = "[";
  size_t count = 0;
  const ScreenDefinition* defs = getAllScreenDefinitions(count);
  ScreenId current = getCurrentScreen();
  bool first = true;
  for (size_t i = 0; i < count; ++i) {
    if (!defs[i].implemented) continue;
    if (!first) json += ",";
    json += "{";
    json += "\"key\":\"" + String(defs[i].key) + "\",";
    json += "\"title\":\"" + String(defs[i].title) + "\",";
    json += "\"current\":" + String(defs[i].id == current ? "true" : "false");
    json += "}";
    first = false;
  }
  json += "]";
  server.send(200, "application/json", json);
}

static void handleAPIStreams() { server.send(200, "application/json", getStreamsList()); }
static void handleAPINetwork() { server.send(200, "application/json", getNetworkInfo()); }

static void handleUploadComplete() {
  const bool pendingImport = g_pendingTarImport;
  const int statusCode = g_uploadHadError ? 500 : (pendingImport ? 202 : 200);
  const String response = g_uploadResponseMessage.length() ? g_uploadResponseMessage : String("Upload complete");
  server.send(statusCode, "text/plain", response);
}

static void handleUploadProcess() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  static size_t totalWritten = 0;
  static String uploadFilename;
  if (upload.status == UPLOAD_FILE_START) {
    String filename = normalizeFsPath(upload.filename);
    g_uploadInProgress = true;
    g_uploadHadError = false;
    g_uploadResponseMessage = "Upload complete";
    uploadFilename = filename;
    totalWritten = 0;
    if (!filename.endsWith(".jpg") && !filename.endsWith(".jpeg") && !filename.endsWith(".png")
        && !filename.endsWith(".csv") && !filename.endsWith(".json") && !filename.endsWith(".tar")) {
      g_uploadHadError = true;
      g_uploadResponseMessage = "Unsupported file type";
      return;
    }
    uploadFile = LittleFS.open(filename, "w", true);
    if (!uploadFile) {
      g_uploadHadError = true;
      g_uploadResponseMessage = "Failed to open file for writing";
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      const size_t written = uploadFile.write(upload.buf, upload.currentSize);
      totalWritten += written;
      if (written != upload.currentSize && !g_uploadHadError) {
        g_uploadHadError = true;
        g_uploadResponseMessage = "Write error while uploading file";
      }
    }
    delay(0);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      String filename = uploadFilename.length() ? uploadFilename : normalizeFsPath(upload.filename);
      if (!g_uploadHadError) {
        if (isImageAssetFilename(filename)) {
          currentBackground = filename;
          loadBackground();
          redrawScreen();
          saveSettings();
        } else if (filename.endsWith(".csv")) {
          loadPlaylistMetadata(filename);
          if (streamCount > 0) selectStream(0);
        } else if (filename.endsWith(".json")) {
          String errorMessage;
          if (!loadDisplayConfigFromFile(filename, &errorMessage)) {
            g_uploadHadError = true;
            g_uploadResponseMessage = "Display config import failed: " + errorMessage;
            Serial.printf("Display config import skipped: %s\n", errorMessage.c_str());
          }
        } else if (filename.endsWith(".tar")) {
          g_pendingTarPath = filename;
          g_pendingTarImport = true;
          g_uploadResponseMessage = "Archive uploaded. Import will continue in background, please wait a few seconds.";
        }
      }
    }
    totalWritten = 0;
    uploadFilename = "";
    g_uploadInProgress = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    if (uploadFilename.length()) LittleFS.remove(uploadFilename);
    totalWritten = 0;
    uploadFilename = "";
    g_uploadInProgress = false;
    g_uploadHadError = true;
    g_uploadResponseMessage = "Upload aborted";
  }
}

static void handleSetColors() {
  if (server.hasArg("station")) colorStation = strtol(server.arg("station").c_str(), nullptr, 16);
  if (server.hasArg("title")) colorTitle = strtol(server.arg("title").c_str(), nullptr, 16);
  if (server.hasArg("bitrate")) colorBitrate = strtol(server.arg("bitrate").c_str(), nullptr, 16);
  if (server.hasArg("volume")) colorVolume = strtol(server.arg("volume").c_str(), nullptr, 16);
  if (server.hasArg("clock")) colorClock = strtol(server.arg("clock").c_str(), nullptr, 16);
  if (server.hasArg("ip")) colorIP = strtol(server.arg("ip").c_str(), nullptr, 16);
  if (server.hasArg("needleMain")) colorNeedleMain = strtol(server.arg("needleMain").c_str(), nullptr, 16);
  if (server.hasArg("needleRed")) colorNeedleRed = strtol(server.arg("needleRed").c_str(), nullptr, 16);
  scheduleDisplaySettingsSave();
  redrawScreen();
  server.send(200, "text/plain", "OK");
}

static void handleUpdateTFT() { redrawScreen(); server.send(200, "text/plain", "TFT Updated"); }

static void handleAPIColors() {
  String json = "{";
  json += "\"station\":" + String(colorStation) + ",";
  json += "\"title\":" + String(colorTitle) + ",";
  json += "\"bitrate\":" + String(colorBitrate) + ",";
  json += "\"volume\":" + String(colorVolume) + ",";
  json += "\"clock\":" + String(colorClock) + ",";
  json += "\"ip\":" + String(colorIP) + ",";
  json += "\"needleMain\":" + String(colorNeedleMain) + ",";
  json += "\"needleRed\":" + String(colorNeedleRed);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleSetNeedleAngles() {
  if (server.hasArg("minAngle")) needleMinAngle = constrain(server.arg("minAngle").toInt(), -360, 0);
  if (server.hasArg("maxAngle")) needleMaxAngle = constrain(server.arg("maxAngle").toInt(), -360, 0);
  server.send(200, "text/plain", "OK");
}

static void handleSetNeedleSpeeds() {
  if (server.hasArg("upSpeed")) needleUpSpeed = constrain(server.arg("upSpeed").toInt(), 1, 20);
  if (server.hasArg("downSpeed")) needleDownSpeed = constrain(server.arg("downSpeed").toInt(), 1, 10);
  server.send(200, "text/plain", "OK");
}

static void handleCommitSettings() {
  flushPendingDisplaySettingsSave();
  server.send(200, "text/plain", "OK");
}

static void handleDownload() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Missing file parameter"); return; }
  String filename = server.arg("file");
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!LittleFS.exists(filename)) { server.send(404, "text/plain", "File not found"); return; }
  File f = LittleFS.open(filename, "r");
  String contentType = "application/octet-stream";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (filename.endsWith(".csv")) contentType = "text/csv";
  else if (filename.endsWith(".json")) contentType = "application/json";
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename.substring(1) + "\"");
  server.streamFile(f, contentType);
  f.close();
}

static void handleSaveDisplayConfig() {
  String filename = server.hasArg("file") ? server.arg("file") : String(getDefaultDisplayConfigFilename());
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!filename.endsWith(".json")) filename += ".json";

  String errorMessage;
  if (!saveDisplayConfigToFile(filename, &errorMessage)) {
    server.send(500, "text/plain", errorMessage);
    return;
  }

  server.send(200, "text/plain", filename);
}

static void handleExportDisplayPackage() {
  String errorMessage;
  if (!buildDisplayPackageTar(kDisplayPackageTempTar, &errorMessage)) {
    server.send(500, "text/plain", errorMessage);
    return;
  }

  File tarFile = LittleFS.open(kDisplayPackageTempTar, "r");
  if (!tarFile) {
    LittleFS.remove(kDisplayPackageTempTar);
    server.send(500, "text/plain", "Failed to open package");
    return;
  }

  server.sendHeader("Content-Disposition", "attachment; filename=\"display_package.tar\"");
  server.streamFile(tarFile, "application/x-tar");
  tarFile.close();
  LittleFS.remove(kDisplayPackageTempTar);
}

static void handleLoadDisplayConfig() {
  String filename = server.hasArg("file") ? server.arg("file") : String(getDefaultDisplayConfigFilename());
  if (!filename.startsWith("/")) filename = "/" + filename;

  String errorMessage;
  if (!loadDisplayConfigFromFile(filename, &errorMessage)) {
    server.send(400, "text/plain", errorMessage);
    return;
  }

  server.send(200, "text/plain", "Display config loaded");
}

static void handleDownloadProfileHeader() {
  String header = buildCurrentProfileHeader();
  server.sendHeader("Content-Disposition", "attachment; filename=\"profile_custom_generated.h\"");
  server.send(200, "text/plain; charset=utf-8", header);
}

static void handleAPIPlaylists() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  while (file) {
    String name = String(file.name());
    if (name.endsWith(".csv")) {
      if (!first) json += ",";
      json += "{\"name\":\"" + name + "\",\"selected\":" + String(name == currentPlaylistFile ? "true" : "false") + "}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

static void handleAPIBackgrounds() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  while (file) {
    String name = String(file.name());
    if (name.endsWith(".jpg") || name.endsWith(".jpeg")) {
      if (!first) json += ",";
      json += "{\"name\":\"" + name + "\",\"selected\":" + String(name == currentBackground ? "true" : "false") + "}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

static void handleViewPlaylist() {
  if (!server.hasArg("pl")) { server.send(400, "text/plain", "Missing filename"); return; }
  String filename = server.arg("pl");
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!LittleFS.exists(filename)) { server.send(404, "text/plain", "Playlist not found"); return; }
  File f = LittleFS.open(filename, "r");
  String json = "[";
  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    if (tab1 == -1 || tab2 == -1) continue;
    if (!first) json += ",";
    json += "{\"name\":\"" + line.substring(0, tab1) + "\",\"url\":\"" + line.substring(tab1 + 1, tab2) + "\"}";
    first = false;
  }
  f.close();
  json += "]";
  server.send(200, "application/json", json);
}

static void handleSavePlaylist() {
  if (!server.hasArg("pl")) { server.send(400, "text/plain", "Missing filename"); return; }
  String filename = server.arg("pl");
  if (!filename.startsWith("/")) filename = "/" + filename;
  String body = server.arg("plain");
  if (!body.length()) { server.send(400, "text/plain", "Missing body"); return; }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "text/plain", String("JSON parse error: ") + err.c_str()); return; }
  File f = LittleFS.open(filename, "w");
  if (!f) { server.send(500, "text/plain", "Failed to open file for writing"); return; }
  for (JsonVariant v : doc.as<JsonArray>()) {
    String name = v["name"] | "";
    String url = v["url"] | "";
    name.replace("\n", " "); name.replace("\r", " "); name.replace("\t", " ");
    url.replace("\n", " "); url.replace("\r", " "); url.replace("\t", " ");
    f.printf("%s\t%s\t0\n", name.c_str(), url.c_str());
  }
  f.close();
  loadPlaylistMetadata(filename);
  server.send(200, "text/plain", "Saved");
}

static void handleAPIAudioInfo() {
  String json = "{";
  json += "\"stationName\":\"" + currentStreamName + "\",";
  json += "\"playlistStation\":\"" + currentStreamName + "\",";
  json += "\"stationNumber\":" + String(currentStreamIndex + 1) + ",";
  json += "\"stationCount\":" + String(streamCount) + ",";
  json += "\"icyStation\":\"" + (icy_station.length() ? icy_station : currentStreamName) + "\",";
  json += "\"icyTitle\":\"" + (icy_streamtitle.length() ? icy_streamtitle : "---") + "\",";
  json += "\"icyBitrate\":\"" + (icy_bitrate.length() ? icy_bitrate : String(audio.getBitRate() / 1000)) + "\",";
  json += "\"codec\":\"" + (audio_codec.length() ? audio_codec : "unknown") + "\",";
  json += "\"frequency\":\"" + (audio_frequency.length() ? audio_frequency : "---") + "\",";
  json += "\"bitness\":\"" + (audio_bitness.length() ? audio_bitness : "---") + "\",";
  json += "\"isPlaying\":" + String(radioStarted && !isPaused ? "true" : "false") + ",";
  json += "\"isPaused\":" + String(isPaused ? "true" : "false") + ",";
  json += "\"volume\":" + String(currentVolume);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleSleepTimer() {
  if (server.hasArg("minutes")) {
    sleepTimer.setTimer(server.arg("minutes").toInt());
    saveSettings();
  }
  server.send(200, "text/plain", "OK");
}

static void handleAPISleepTimer() {
  String json = "{";
  json += "\"active\":" + String(sleepTimer.isTimerActive() ? "true" : "false") + ",";
  json += "\"duration\":" + String(sleepTimer.getSleepDuration()) + ",";
  json += "\"remaining\":" + String(sleepTimer.getRemainingMinutes());
  json += "}";
  server.send(200, "application/json", json);
}

static void handleAPIEq() {
  String json = "{";
  json += "\"balance\":" + String(eqBalance) + ",";
  json += "\"treble\":" + String(eqTreble) + ",";
  json += "\"mid\":" + String(eqMid) + ",";
  json += "\"bass\":" + String(eqBass);
  json += "}";
  server.send(200, "application/json", json);
}

static void handleSetRotation() {
  if (server.hasArg("rotation")) {
    int newRotation = server.arg("rotation").toInt();
    if (newRotation >= 0 && newRotation <= 3) {
      tftRotation = newRotation;
      gfx->setRotation(tftRotation);
      scheduleDisplaySettingsSave();
      redrawScreen();
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleAPIRotation() {
  String json = "{\"rotation\":" + String(tftRotation) + "}";
  server.send(200, "application/json", json);
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/thems.css", handleThemsCss);
  server.on("/setbrightness", handleSetBrightness);
  server.on("/api/brightness", handleAPIBrightness);
  server.on("/setup", handleWiFiSetupPage);
  server.on("/ota", handleOTAPage);
  server.on("/scan", handleWiFiScan);
  server.on("/savewifi", handleWiFiSave);
  server.on("/setcolors", handleSetColors);
  server.on("/updatetft", handleUpdateTFT);
  server.on("/visualreset", handleVisualReset);
  server.on("/download", handleDownload);
  server.on("/savedisplayconfig", handleSaveDisplayConfig);
  server.on("/exportdisplaypackage", handleExportDisplayPackage);
  server.on("/loaddisplayconfig", handleLoadDisplayConfig);
  server.on("/downloadprofileheader", handleDownloadProfileHeader);
  server.on("/viewplaylist", handleViewPlaylist);
  server.on("/saveplaylist", HTTP_POST, handleSavePlaylist);
  server.on("/setneedlespeeds", handleSetNeedleSpeeds);
  server.on("/setneedleangles", handleSetNeedleAngles);
  server.on("/api/colors", handleAPIColors);
  server.on("/next", handleNext);
  server.on("/prev", handlePrev);
  server.on("/play", handlePlay);
  server.on("/setled", handleSetLed);
  server.on("/setvumeter", handleSetVuMeter);
  server.on("/setvubars", handleSetVuBars);
  server.on("/setvucardio", handleSetVuCardio);
  server.on("/setmarquee1", handleSetMarquee1);
  server.on("/setmarquee2", handleSetMarquee2);
  server.on("/setstationfont", handleSetStationFont);
  server.on("/setquote", handleSetQuote);
  server.on("/setledconfig", handleSetLedConfig);
  server.on("/playurl", handlePlayURL);
  server.on("/selectscreen", handleSelectScreen);
  server.on("/setvolume", handleSetVolume);
  server.on("/seteq", handleSetEq);
  server.on("/setneedle", handleSetNeedle);
  server.on("/setneedleapp", handleSetNeedleAppearance);
  server.on("/settextpos", handleSetTextPos);
  server.on("/commitsettings", handleCommitSettings);
  server.on("/selectstream", handleSelectStream);
  server.on("/selectplaylist", handleSelectPlaylist);
  server.on("/myplaylist", handleMyPlaylist);
  server.on("/addtomylist", handleAddToMyPlaylist);
  server.on("/selectbg", handleSelectBg);
  server.on("/deletebg", handleDeleteBg);
  server.on("/deleteplaylist", handleDeletePlaylist);
  server.on("/api/audioinfo", handleAPIAudioInfo);
  server.on("/api/player", handleAPIPlayer);
  server.on("/api/led", handleAPILed);
  server.on("/api/vumeter", handleAPIVuMeter);
  server.on("/api/marquee1", handleAPIMarquee1);
  server.on("/api/marquee2", handleAPIMarquee2);
  server.on("/api/stationfont", handleAPIStationFont);
  server.on("/api/quote", handleAPIQuote);
  server.on("/api/screens", handleAPIScreens);
  server.on("/api/streams", handleAPIStreams);
  server.on("/api/backgrounds", handleAPIBackgrounds);
  server.on("/api/playlists", handleAPIPlaylists);
  server.on("/api/network", handleAPINetwork);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadProcess);
  server.on("/update", HTTP_POST, handleOTAUpdateResult, handleOTAUpload);
  server.on("/sleeptimer", handleSleepTimer);
  server.on("/api/sleeptimer", handleAPISleepTimer);
  server.on("/api/eq", handleAPIEq);
  server.on("/setrotation", handleSetRotation);
  server.on("/api/rotation", handleAPIRotation);
  server.onNotFound(handleNotFound);
  server.begin();
  //Serial.println("Web server started!");
  Serial.print("Open: http://");
  Serial.print(WiFi.localIP());
  Serial.println();
}
