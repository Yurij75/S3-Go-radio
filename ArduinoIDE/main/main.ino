#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Audio.h>
#include <TJpg_Decoder.h>
#include <Preferences.h>
#include "time.h"
#include "DirectiveFour30.h"
#include "FreeSerifBoldItalic14pt8b.h"
#include <LittleFS.h>
#include "config.h"
#include "web_interface.h"
#include "wifi_setup.h"
#include <ArduinoJson.h>


// === Preferences для сохранения настроек ===
Preferences prefs;

// === Переменные для метаданных потока ===
String icy_station = "";
String icy_streamtitle = "";
String icy_url = "";
String icy_bitrate = "";
String id3_data = "";

// === Text Colors ===
uint16_t colorStation = COLOR_STATION;
uint16_t colorTitle = COLOR_TITLE; 
uint16_t colorBitrate = COLOR_BITRATE;
uint16_t colorVolume = COLOR_VOLUME;
uint16_t colorClock = COLOR_CLOCK;
uint16_t colorNeedleMain = COLOR_NEEDLE_MAIN;
uint16_t colorNeedleRed = COLOR_NEEDLE_RED;

// --- TFT Display - Auto-select based on config.h ---
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);

#if defined(DISPLAY_ST7796)
  Arduino_GFX *gfx = new Arduino_ST7796(bus, TFT_RST);

#elif defined(DISPLAY_ILI9341)
  Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, tftRotation);  

#elif defined(DISPLAY_ST7789)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 3, true);

#elif defined(DISPLAY_ST7789_172_3)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 172, 320, 34, 0);

#elif defined(DISPLAY_ST7789_172_0)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 172, 320, 34, 0);

#elif defined(DISPLAY_ST7789_76_0)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, false, 76, 284, 82, 18);

#elif defined(DISPLAY_ST7789_76_3)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, false, 76, 320, 82, 18);

#else
  #error "No display type defined! Please uncomment one DISPLAY_* in config.h"
#endif

Arduino_Canvas *bgSprite;
Arduino_Canvas *needleSpriteL;
Arduino_Canvas *needleSpriteR;
Arduino_Canvas *clockSprite;
int clockW, clockH;
int clockSpriteX, clockSpriteY;


Audio audio;
WebServer server(80);

// --- Global variables ---
int vuWidth, vuHeight;
bool wifiConnected = false, radioStarted = false;
bool isPaused = false;
SemaphoreHandle_t xMutex;

// --- Deferred Play/Pause flags ---
bool pendingToggle = false;

// === Metadata waiting flags ===
bool metadataPending = false;
unsigned long metadataRequestTime = 0;
const unsigned long METADATA_TIMEOUT = 3000; // 3 секунды таймаут

// --- Needles ---
int current_needle_L = 0, target_needle_L = 0;
int current_needle_R = 0, target_needle_R = 0;

// --- Needle positions ---
int needlePosLX = NEEDLE_L_X;
int needlePosLY = NEEDLE_L_Y;
int needlePosRX = NEEDLE_R_X;
int needlePosRY = NEEDLE_R_Y;

// === Needle Animation ===
int needleUpSpeed = NEEDLE_UP_SPEED;
int needleDownSpeed = NEEDLE_DOWN_SPEED;

// --- Needle settings ---
int needleThickness = 4;      // Толщина стрелки
int needleLenMain = 60;       // Длина основной части
int needleLenRed = 25;        // Длина красной части
int needleCY = SPRITE_HEIGHT; // Положение центра вращения по Y

// === Needle Angles ===
int needleMinAngle = NEEDLE_MIN_ANGLE;
int needleMaxAngle = NEEDLE_MAX_ANGLE;

// --- Volume ---
int currentVolume = AUDIO_DEFAULT_VOLUME;

// --- Clock ---
String prevClock = "--:--:--";
String prevBitrate = "";
bool redrawClock = true;
String prevStreamName = "";

// --- Background filename ---
String currentBackground = "/00.jpg";

// === Text positions (настраиваемые координаты) ===
int stationNameX = 15, stationNameY = 124;
int streamTitle1X = 15, streamTitle1Y = 200;
int streamTitle2X = 15, streamTitle2Y = 270;
int bitrateX = 15, bitrateY = 345;
int clockX = 120, clockY = 30;  // X будет пересчитан для центрирования
int volumeX = 200, volumeY = 345;

// === LAZY PLAYLIST LOADING ===
struct PlaylistEntry {
  String name;
};

PlaylistEntry playlistIndex[100];
int streamCount = 0;
String currentPlaylistFile = "/playlist.csv";
String currentStreamURL = "";
String currentStreamName = "";
int currentStreamIndex = 0;
volatile bool streamChangeRequested = false;
unsigned long lastStreamChangeMillis = 0;

// === Forward Declarations ===
void redrawScreen();
void loadBackground();
void startRadio();
void saveSettings();
void loadSettings();

// === Колбэк для вывода ICY / ID3 метаданных ===
void myAudioInfo(Audio::msg_t m) {
  switch (m.e) {
    case Audio::evt_name:
      Serial.printf("[ICY] Station: %s\n", m.msg);
      icy_station = String(m.msg);
      metadataPending = false; // Метаданные получены
      redrawScreen(); // Немедленная перерисовка
      break;
    case Audio::evt_streamtitle:
      Serial.printf("[ICY] Now playing: %s\n", m.msg);
      icy_streamtitle = String(m.msg);
      metadataPending = false; // Метаданные получены
      redrawScreen(); // Немедленная перерисовка
      break;
    case Audio::evt_icyurl:
      Serial.printf("[ICY] URL: %s\n", m.msg);
      icy_url = String(m.msg);
      break;
    case Audio::evt_bitrate:
      Serial.printf("[ICY] Bitrate: %s kbps\n", m.msg);
      icy_bitrate = String(m.msg);
      metadataPending = false; // Метаданные получены
      redrawScreen(); // Немедленная перерисовка
      break;
    case Audio::evt_id3data:
      Serial.printf("[ID3] Tag: %s\n", m.msg);
      id3_data = String(m.msg);
      break;
    default:
      break;
  }
}

// === BUTTON CONTROL ===
volatile bool flagVolUp = false;
volatile bool flagVolDown = false;
volatile bool flagNext = false;
volatile bool flagPrev = false;
volatile bool flagPlayPause = false;

volatile unsigned long lastVolUpISR = 0;
volatile unsigned long lastVolDownISR = 0;
volatile unsigned long lastNextISR = 0;
volatile unsigned long lastPrevISR = 0;
volatile unsigned long lastPlayPauseISR = 0;

const unsigned long DEBOUNCE_MS = 300;

// === ISR (обработчики прерываний) ===
void IRAM_ATTR volUpISR() {
  unsigned long now = millis();
  if (now - lastVolUpISR > DEBOUNCE_MS) {
    lastVolUpISR = now;
    flagVolUp = true;
  }
}

void IRAM_ATTR volDownISR() {
  unsigned long now = millis();
  if (now - lastVolDownISR > DEBOUNCE_MS) {
    lastVolDownISR = now;
    flagVolDown = true;
  }
}

void IRAM_ATTR prevStreamISR() {
  unsigned long now = millis();
  if (now - lastPrevISR > DEBOUNCE_MS) {
    lastPrevISR = now;
    flagPrev = true;
  }
}

void IRAM_ATTR nextStreamISR() {
  unsigned long now = millis();
  if (now - lastNextISR > DEBOUNCE_MS) {
    lastNextISR = now;
    flagNext = true;
  }
}

void IRAM_ATTR playPauseISR() {
  unsigned long now = millis();
  if (now - lastPlayPauseISR > DEBOUNCE_MS) {
    lastPlayPauseISR = now;
    flagPlayPause = true;
  }
}

// === Settings Save/Load ===
void saveSettings() {
  prefs.begin("radio", false);
  prefs.putString("bg", currentBackground);
  prefs.putString("playlist", currentPlaylistFile);
  prefs.putInt("stream", currentStreamIndex);
  prefs.putString("streamURL", currentStreamURL);
  prefs.putString("streamName", currentStreamName);
  prefs.putInt("volume", currentVolume);
  
  // Needle positions
  prefs.putInt("needleLX", needlePosLX);
  prefs.putInt("needleLY", needlePosLY);
  prefs.putInt("needleRX", needlePosRX);
  prefs.putInt("needleRY", needlePosRY);

  // Needle animation speeds
  prefs.putInt("needleUpSpeed", needleUpSpeed);
  prefs.putInt("needleDownSpeed", needleDownSpeed);

  // Needle angles
  prefs.putInt("needleMinAngle", needleMinAngle);
  prefs.putInt("needleMaxAngle", needleMaxAngle);

  // Text colors
  prefs.putUShort("colorStation", colorStation);
  prefs.putUShort("colorTitle", colorTitle);
  prefs.putUShort("colorBitrate", colorBitrate);
  prefs.putUShort("colorVolume", colorVolume);
  prefs.putUShort("colorClock", colorClock);
  prefs.putUShort("colorNeedleMain", colorNeedleMain);
  prefs.putUShort("colorNeedleRed", colorNeedleRed);
  
  // Needle settings
  prefs.putInt("needleThk", needleThickness);
  prefs.putInt("needleMain", needleLenMain);
  prefs.putInt("needleRed", needleLenRed);
  prefs.putInt("needleCY", needleCY);
  
  // Text positions
  prefs.putInt("stationX", stationNameX);
  prefs.putInt("stationY", stationNameY);
  prefs.putInt("title1X", streamTitle1X);
  prefs.putInt("title1Y", streamTitle1Y);
  prefs.putInt("title2X", streamTitle2X);
  prefs.putInt("title2Y", streamTitle2Y);
  prefs.putInt("bitrateX", bitrateX);
  prefs.putInt("bitrateY", bitrateY);
  prefs.putInt("clockY", clockY);
  prefs.putInt("volumeX", volumeX);
  prefs.putInt("volumeY", volumeY);
  
  prefs.end();
  Serial.println("✅ Settings saved");
  Serial.printf("💾 Saved station: idx=%d, name=%s\n", currentStreamIndex, currentStreamName.c_str());
  Serial.printf("💾 Saved URL: %s\n", currentStreamURL.c_str());
}

void loadSettings() {
  prefs.begin("radio", true);
  
  currentBackground = prefs.getString("bg", "/00.jpg");
  currentPlaylistFile = prefs.getString("playlist", "/playlist.csv");
  currentStreamIndex = prefs.getInt("stream", 0);
  currentVolume = prefs.getInt("volume", AUDIO_DEFAULT_VOLUME);
  
  // Needle positions
  needlePosLX = prefs.getInt("needleLX", NEEDLE_L_X);
  needlePosLY = prefs.getInt("needleLY", NEEDLE_L_Y);
  needlePosRX = prefs.getInt("needleRX", NEEDLE_R_X);
  needlePosRY = prefs.getInt("needleRY", NEEDLE_R_Y);

  // Needle animation speeds
  needleUpSpeed = prefs.getInt("needleUpSpeed", NEEDLE_UP_SPEED);
  needleDownSpeed = prefs.getInt("needleDownSpeed", NEEDLE_DOWN_SPEED);
  
  // Needle settings
  needleThickness = prefs.getInt("needleThk", 4);
  needleLenMain = prefs.getInt("needleMain", 60);
  needleLenRed = prefs.getInt("needleRed", 25);
  needleCY = prefs.getInt("needleCY", SPRITE_HEIGHT);

  // Needle angles
  needleMinAngle = prefs.getInt("needleMinAngle", NEEDLE_MIN_ANGLE);
  needleMaxAngle = prefs.getInt("needleMaxAngle", NEEDLE_MAX_ANGLE);
  
  // Text positions
  stationNameX = prefs.getInt("stationX", 15);
  stationNameY = prefs.getInt("stationY", 124);
  streamTitle1X = prefs.getInt("title1X", 15);
  streamTitle1Y = prefs.getInt("title1Y", 200);
  streamTitle2X = prefs.getInt("title2X", 15);
  streamTitle2Y = prefs.getInt("title2Y", 270);
  bitrateX = prefs.getInt("bitrateX", 15);
  bitrateY = prefs.getInt("bitrateY", 345);
  clockY = prefs.getInt("clockY", 30);
  volumeX = prefs.getInt("volumeX", 200);
  volumeY = prefs.getInt("volumeY", 345);

  // Text colors
  colorStation = prefs.getUShort("colorStation", COLOR_STATION);
  colorTitle = prefs.getUShort("colorTitle", COLOR_TITLE);
  colorBitrate = prefs.getUShort("colorBitrate", COLOR_BITRATE);
  colorVolume = prefs.getUShort("colorVolume", COLOR_VOLUME);
  colorClock = prefs.getUShort("colorClock", COLOR_CLOCK);
  colorNeedleMain = prefs.getUShort("colorNeedleMain", COLOR_NEEDLE_MAIN);
  colorNeedleRed = prefs.getUShort("colorNeedleRed", COLOR_NEEDLE_RED);
  
  prefs.end();
  Serial.println("✅ Settings loaded");
}

// === WiFi ===
bool connectToWiFi() {
  // Load saved WiFi credentials
  Preferences prefs;
  prefs.begin("radio", true);
  String saved_ssid = prefs.getString("wifi_ssid", "");
  String saved_pass = prefs.getString("wifi_pass", "");
  prefs.end();
  
  // If no saved credentials, start setup AP
  if (saved_ssid.length() == 0) {
    Serial.println("No WiFi credentials found - starting setup AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("S3-Go!-light-Setup", "12345678");
    Serial.print("Setup AP IP: ");
    Serial.println(WiFi.softAPIP());
    wifiConnected = false;
    return false;
  }
  
  // Try to connect with saved credentials
  WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
  WiFi.setSleep(false);
  Serial.print("Connecting WiFi: ");
  Serial.println(saved_ssid.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    return true;
  } else {
    Serial.println("\nWiFi connection failed - starting setup AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("S3-Go!-light-Setup", "12345678");
    Serial.print("Setup AP IP: ");
    Serial.println(WiFi.softAPIP());
    wifiConnected = false;
    return false;
  }
}

// === Playlist - Lazy Loading ===
bool getStreamByIndex(int idx, String &outName, String &outURL) {
  if(idx < 0 || idx >= streamCount) return false;
  
  File f = LittleFS.open(currentPlaylistFile, "r");
  if(!f) return false;
  
  int lineNum = 0;
  while(f.available() && lineNum < idx) {
    int ch = f.read();
    if(ch == '\n') lineNum++;
  }
  
  if(lineNum < idx) {
    f.close();
    return false;
  }
  
  String line = f.readStringUntil('\n');
  line.trim();
  f.close();
  
  if(line.length() == 0) return false;
  
  int tab1 = line.indexOf('\t');
  int tab2 = line.indexOf('\t', tab1 + 1);
  
  if(tab1 == -1 || tab2 == -1) return false;
  
  outName = line.substring(0, tab1);
  outURL = line.substring(tab1 + 1, tab2);
  
  Serial.printf("✅ Loaded stream #%d: %s -> %s\n", idx, outName.c_str(), outURL.c_str());
  return true;
}

void loadPlaylistMetadata(String filename) {
  if (!filename.startsWith("/")) filename = "/" + filename;

  if (!LittleFS.exists(filename)) {
    Serial.printf("⚠️ Playlist file %s not found - creating empty playlist\n", filename.c_str());
    currentPlaylistFile = filename;
    streamCount = 0;
    return;
  }

  currentPlaylistFile = filename;
  streamCount = 0;
  
  File f = LittleFS.open(filename, "r");
  if (!f) {
    Serial.printf("⚠️ Failed to open playlist file %s\n", filename.c_str());
    return;
  }
  
  while (f.available() && streamCount < 100) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    if (tab1 == -1 || tab2 == -1) continue;
    
    playlistIndex[streamCount].name = line.substring(0, tab1);
    streamCount++;
  }
  
  f.close();
  
  Serial.printf("✅ Playlist loaded: %d streams (metadata only, URLs on demand)\n", streamCount);
  // currentStreamIndex = 0;
}

// === Обновленная функция selectStream ===
void selectStream(int idx) {
  if(idx < 0 || idx >= streamCount) return;
  
  currentStreamIndex = idx;
  
  if(getStreamByIndex(idx, currentStreamName, currentStreamURL)) {
    // Сбрасываем метаданные перед сменой потока
    icy_station = "";
    icy_streamtitle = "";
    icy_bitrate = "";
    
    // Устанавливаем флаг ожидания метаданных
    metadataPending = true;
    metadataRequestTime = millis();
    
    audio.connecttohost(currentStreamURL.c_str());
    radioStarted = true;
    isPaused = false;
    
    // Немедленная перерисовка с очищенными метаданными
    redrawScreen();
    saveSettings();
    
    Serial.printf("🔄 Switched to #%d: %s\n", idx, currentStreamName.c_str());
    Serial.println("⏳ Waiting for metadata...");
  }
}


String getStreamName(int idx) {
  if(idx >= 0 && idx < streamCount) {
    return playlistIndex[idx].name;
  }
  return "Unknown";
}

void nextStream() {
  if(streamCount > 0){
    currentStreamIndex++;
    if(currentStreamIndex >= streamCount) currentStreamIndex = 0;
    selectStream(currentStreamIndex);
  }
}

void prevStream() {
  if(streamCount > 0){
    currentStreamIndex--;
    if(currentStreamIndex < 0) currentStreamIndex = streamCount - 1;
    selectStream(currentStreamIndex);
  }
}

void checkStreamChange() {
  if (streamChangeRequested && streamCount > 0) {
    nextStream();
    streamChangeRequested = false;
  }
}

// === Helper: Trim text by pixel width ===
String trimTextToWidth(Arduino_GFX *gfx, String text, int maxWidth) {
  int16_t x1, y1;
  uint16_t w, h;

  // сохраняем оригинал, чтобы знать, обрезали ли
  String original = text;

  // уменьшаем строку, пока не влезет
  while (text.length() > 0) {
    gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth) break;
    text.remove(text.length() - 1);
  }

  // если строка была обрезана — добавляем "..."
  if (text.length() < original.length()) {
    String withDots = text + "...";
    gfx->getTextBounds(withDots.c_str(), 0, 0, &x1, &y1, &w, &h);

    // если не помещается даже с троеточием — ещё подрежем
    while (w > maxWidth && text.length() > 0) {
      text.remove(text.length() - 1);
      withDots = text + "...";
      gfx->getTextBounds(withDots.c_str(), 0, 0, &x1, &y1, &w, &h);
    }

    text = withDots;
  }

  return text;
}

// === Helper: Draw metadata lines (исправленная) ===
void drawDataLines() {
  gfx->setFont(&FreeSerifBoldItalic14pt8b);
  
  const int maxTextWidth = 260;

  // === Line 1: Station name ===
  gfx->setTextColor(colorStation);
  gfx->setCursor(stationNameX, stationNameY);
  String line1 = icy_station.length() > 0 ? icy_station : "---";
  line1 = trimTextToWidth(gfx, line1, maxTextWidth);
  gfx->print(line1);

  // === Line 2: Stream title ===
  gfx->setTextColor(colorTitle);
  String line2 = icy_streamtitle.length() > 0 ? icy_streamtitle : "---";
  // ... остальной код без изменений

  if (line2.indexOf('-') != -1) {
    int dashPos = line2.indexOf('-');
    String firstLine = line2.substring(0, dashPos);
    String secondLine = line2.substring(dashPos + 1);

    firstLine.trim();
    secondLine.trim();

    firstLine = trimTextToWidth(gfx, firstLine, maxTextWidth);
    secondLine = trimTextToWidth(gfx, secondLine, maxTextWidth);

    gfx->setCursor(streamTitle1X, streamTitle1Y);
    gfx->print(firstLine);

    gfx->setCursor(streamTitle2X, streamTitle2Y);
    gfx->print(secondLine);
  } else {
    line2 = trimTextToWidth(gfx, line2, maxTextWidth);
    gfx->setCursor(streamTitle1X, streamTitle1Y);
    gfx->print(line2);
  }

  // === Line 3: Bitrate ===
  gfx->setTextColor(colorBitrate);  // ОТДЕЛЬНЫЙ цвет для битрейта
  gfx->setCursor(bitrateX, bitrateY);
  int br = audio.getBitRate() / 1000;

  String line3;
  if (br > 0) {
      line3 = String(br) + " kbps";     // ← Реальный битрейт
  } else if (icy_bitrate.length() > 0) {
      line3 = icy_bitrate + " kbps";    // ← ICY-метаданные (MP3)
  } else {
      line3 = "---";                    // ← Нет информации
  }

  line3 = trimTextToWidth(gfx, line3, maxTextWidth);
  gfx->print(line3);
}


// Вывод громкости
void drawVolumeDisplay() {
  #ifdef SHOW_VOLUME_DISPLAY
    if (SHOW_VOLUME_DISPLAY) {
      gfx->setFont(&FreeSerifBoldItalic14pt8b);
      gfx->setTextColor(colorVolume);
      String text = "Vol: " + String(currentVolume);
      gfx->setCursor(volumeX, volumeY);
      gfx->print(text);
    }
  #endif
}

// === Radio ===
// === Обновленная функция startRadio ===
void startRadio() {
  if (!wifiConnected || streamCount == 0) return;

  if (radioStarted && isPaused) {
    audio.pauseResume();
    isPaused = false;
    Serial.println("▶️ Radio resumed");
    redrawScreen();
    return;
  }

  if (!radioStarted) {
    audio.setPinout(AUDIO_I2S_BCLK, AUDIO_I2S_LRCLK, AUDIO_I2S_DOUT);
    
    // Сбрасываем метаданные при старте
    icy_station = "";
    icy_streamtitle = "";
    icy_bitrate = "";
    metadataPending = true;
    metadataRequestTime = millis();
    
    selectStream(currentStreamIndex);
    audio.setVolume(currentVolume);
    radioStarted = true;
    isPaused = false;
    Serial.println("▶️ Radio started");
    redrawScreen();
  }
}

// === NTP ===
void initClock() {
  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
}

// === Clock ===
void drawClockHMSS() {
  static int lastSec = -1;
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  if (timeinfo.tm_sec == lastSec) return;
  lastSec = timeinfo.tm_sec;

  char clockBuf[10];
  sprintf(clockBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // копируем фон под часами из bgSprite
  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* clkBuf = (uint16_t*)clockSprite->getFramebuffer();

  for (int y = 0; y < clockH; y++) {
    int srcY = clockY + y;
    if (srcY >= 0 && srcY < vuHeight) {
      memcpy(&clkBuf[y * clockW], &bgBuf[srcY * vuWidth + clockSpriteX],
            clockW * sizeof(uint16_t));
    } else {
      memset(&clkBuf[y * clockW], 0, clockW * sizeof(uint16_t));
    }
  }

  clockSprite->setTextColor(colorClock);
  clockSprite->setFont(&DirectiveFour30);  //шрифт часов

  int16_t x1, y1;
  uint16_t w, h;
  clockSprite->getTextBounds(clockBuf, 0, 0, &x1, &y1, &w, &h);
  int x = (clockW - w) / 2;
  int y = (clockH + h) / 2;
  clockSprite->setCursor(x, y);
  clockSprite->print(clockBuf);

  // Вывод на экран
  gfx->draw16bitRGBBitmap(clockSpriteX, clockY,
                        (uint16_t*)clockSprite->getFramebuffer(),
                        clockW, clockH);
    // Добавь контур спрайта часов
  // gfx->drawRect(clockSpriteX, clockY, clockW, clockH, RGB565_CYAN);
}

// === ON-SCREEN FEEDBACK ===
unsigned long lastActionTime = 0;

void showAction(String text, uint16_t color = RGB565_CYAN) {
  gfx->setFont(&FreeSerifBoldItalic14pt8b);
  gfx->setTextColor(color, RGB565_BLACK);
  gfx->fillRect(0, gfx->height() - 40, gfx->width(), 30, RGB565_BLACK);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->print(text);
  lastActionTime = millis();
}

void clearActionIfTimeout() {
  if (lastActionTime != 0 && millis() - lastActionTime > 2000) {
    gfx->fillRect(0, gfx->height() - 40, gfx->width(), 30, RGB565_BLACK);
    lastActionTime = 0;
  }
}

// === VU meters ===
void updateVULevels() {
  if (!radioStarted || isPaused) {
    target_needle_L = target_needle_R = 0;
  } else {
    int vu = audio.getVUlevel();
    uint8_t L = (vu >> 8) & 0xFF;
    uint8_t R = vu & 0xFF;
    target_needle_L = constrain(map(L, NEEDLE_MIN_VALUE, NEEDLE_MAX_VALUE, 0, 100), 0, 100);
    target_needle_R = constrain(map(R, NEEDLE_MIN_VALUE, NEEDLE_MAX_VALUE, 0, 100), 0, 100);
  }

  // Используем настраиваемые скорости
  const int up = needleUpSpeed;
  const int down = needleDownSpeed;
  current_needle_L += (current_needle_L < target_needle_L) ? up : -down;
  current_needle_R += (current_needle_R < target_needle_R) ? up : -down;
  current_needle_L = constrain(current_needle_L, 0, 100);
  current_needle_R = constrain(current_needle_R, 0, 100);
}

// === Sprites ===
void createSprites() {
  vuWidth  = gfx->width();
  vuHeight = gfx->height();
  int W = SPRITE_WIDTH, H = SPRITE_HEIGHT;

  // --- фон ---
  bgSprite = new Arduino_Canvas(vuWidth, vuHeight, gfx);
  bgSprite->begin();

  // --- стрелки ---
  needleSpriteL = new Arduino_Canvas(W, H, gfx);
  needleSpriteL->begin();
  needleSpriteR = new Arduino_Canvas(W, H, gfx);
  needleSpriteR->begin();

  // --- часы ---
  gfx->setFont(&DirectiveFour30);

  // тестовая строка для определения размеров
  char testStr[] = "88:88:88";
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(testStr, 0, 0, &x1, &y1, &w, &h);

  // немного запаса по краям
  clockW = w + 6;
  clockH = h + 6;

  // создаём спрайт часов
  clockSprite = new Arduino_Canvas(clockW, clockH, gfx);
  clockSprite->begin();

  // позиция по центру экрана
  clockSpriteX = (gfx->width() - clockW) / 2;
  clockSpriteY = clockY;

  // дублируем координаты для совместимости со старым кодом
  clockX = clockSpriteX;
  clockY = clockSpriteY;

  // заливаем фон спрайта часами из bgSprite (чтобы не был черный)
  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* clkBuf = (uint16_t*)clockSprite->getFramebuffer();

  for (int y = 0; y < clockH; y++) {
    int srcY = clockSpriteY + y;
    if (srcY >= 0 && srcY < vuHeight) {
      memcpy(&clkBuf[y * clockW],
             &bgBuf[srcY * vuWidth + clockSpriteX],
             clockW * sizeof(uint16_t));
    } else {
      memset(&clkBuf[y * clockW], 0, clockW * sizeof(uint16_t));
    }
  }
}

// === TJpg callback ===
bool tpgOutputToSprite(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= vuHeight) return 0;
  bgSprite->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

// === Load background ===
void loadBackground() {
  Serial.printf("Loading: %s\n", currentBackground.c_str());
  if (!LittleFS.exists(currentBackground)) {
    Serial.println("Background file not found - filling with default color");
    bgSprite->fillScreen(RGB565_BLACK);
    return;
  }
  File file = LittleFS.open(currentBackground, "r");
  if (!file) {
    Serial.println("Failed to open file - filling with default color");
    bgSprite->fillScreen(RGB565_BLACK);
    return;
  }
  size_t fileSize = file.size();
  uint8_t* jpegData = (uint8_t*)ps_malloc(fileSize);
  if (!jpegData) {
    Serial.println("Memory allocation failed");
    bgSprite->fillScreen(RGB565_BLACK);
    file.close();
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
  Serial.println("Background loaded!");
}

// === Draw VU needles ===
void drawVuNeedleChannel(Arduino_Canvas* needleSprite, int posX, int posY, int vuLevel) {
  int W = needleSprite->width();
  int H = needleSprite->height();
  uint16_t* bgBuf = (uint16_t*)bgSprite->getFramebuffer();
  uint16_t* needleBuf = (uint16_t*)needleSprite->getFramebuffer();

  for (int y = 0; y < H; y++)
    memcpy(&needleBuf[y * W], &bgBuf[(posY + y) * vuWidth + posX], W * sizeof(uint16_t));
  int cx = W / 2;
  int cy = needleCY;
  
  // Используем настраиваемые углы
  float angle = map(vuLevel, 0, 100, needleMinAngle, needleMaxAngle) * 3.14159265 / 180.0;
  float cosA = cos(angle), sinA = sin(angle);

  // Остальной код без изменений...
  for(int t = -needleThickness/2; t <= needleThickness/2; t++) {
    int offsetX = t * sin(angle);
    int offsetY = -t * cos(angle);
    
    // Основная часть
    needleSprite->drawLine(
      cx + offsetX, 
      cy + offsetY, 
      cx + (needleLenMain - needleLenRed) * cosA + offsetX, 
      cy + (needleLenMain - needleLenRed) * sinA + offsetY, 
      colorNeedleMain
    );
    
    // Красная часть
    needleSprite->drawLine(
      cx + (needleLenMain - needleLenRed) * cosA + offsetX, 
      cy + (needleLenMain - needleLenRed) * sinA + offsetY,
      cx + needleLenMain * cosA + offsetX, 
      cy + needleLenMain * sinA + offsetY, 
      colorNeedleRed
    );
  }
                       
  gfx->draw16bitRGBBitmap(posX, posY, needleBuf, W, H);

  bool debugBorders = DEBUG_BORDERS;
  if (debugBorders) {
    gfx->drawRect(needlePosLX, needlePosLY, needleSpriteL->width(), needleSpriteL->height(), RGB565_RED);
    gfx->drawRect(needlePosRX, needlePosRY, needleSpriteR->width(), needleSpriteR->height(), RGB565_BLUE);
  }
}

void displayTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(500));
  
  while (true) {
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      if (bgSprite != NULL && needleSpriteL != NULL && needleSpriteR != NULL) {
        updateVULevels();
        drawVuNeedleChannel(needleSpriteL, needlePosLX, needlePosLY, current_needle_L);
        drawVuNeedleChannel(needleSpriteR, needlePosRX, needlePosRY, current_needle_R);
        drawClockHMSS();
      }
      xSemaphoreGive(xMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_TASK_DELAY));
  }
}

// === Redraw screen ===
// === Обновленная функция redrawScreen ===
void redrawScreen() {
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)bgSprite->getFramebuffer(), vuWidth, vuHeight);
    drawDataLines();
    
    if (isPaused) {
      gfx->setFont(&FreeSerifBoldItalic14pt8b);
      gfx->setTextColor(RGB565_ORANGE);
      int16_t x1, y1;
      uint16_t w, h;
      gfx->getTextBounds("PAUSED", 0, 0, &x1, &y1, &w, &h);
      int centerX = (gfx->width() - w) / 2;
      gfx->setCursor(centerX, 180);
      gfx->print("PAUSED");
    } else if (!radioStarted) {
      gfx->setFont(&FreeSerifBoldItalic14pt8b);
      gfx->setTextColor(RGB565_RED);
      int16_t x1, y1;
      uint16_t w, h;
      gfx->getTextBounds("STOPPED", 0, 0, &x1, &y1, &w, &h);
      int centerX = (gfx->width() - w) / 2;
      gfx->setCursor(centerX, 180);
      gfx->print("STOPPED");
    } else if (metadataPending) {
      // Показываем индикатор загрузки метаданных
      gfx->setFont(&FreeSerifBoldItalic14pt8b);
      gfx->setTextColor(RGB565_YELLOW);
      int16_t x1, y1;
      uint16_t w, h;
      gfx->getTextBounds("LOADING...", 0, 0, &x1, &y1, &w, &h);
      int centerX = (gfx->width() - w) / 2;
      gfx->setCursor(centerX, 180);
      gfx->print("LOADING...");
    }
    
    prevStreamName = currentStreamName;
    redrawClock = true;
    drawClockHMSS();
    drawVolumeDisplay();
    clearActionIfTimeout();
    
    xSemaphoreGive(xMutex);
  }
}

// === Delete Handlers ===
void handleDeleteBg() {
  if(server.hasArg("bg")){
    String filename = server.arg("bg");
    if(!filename.startsWith("/")) filename = "/" + filename;
    
    if(LittleFS.exists(filename) && (filename.endsWith(".jpg") || filename.endsWith(".jpeg"))){
      LittleFS.remove(filename);
      Serial.printf("Deleted background: %s\n", filename.c_str());
      
      if(currentBackground == filename){
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
          String name = String(file.name());
          if(name.endsWith(".jpg") || name.endsWith(".jpeg")){
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

void handleDeletePlaylist() {
  if(server.hasArg("pl")){
    String filename = server.arg("pl");
    if(!filename.startsWith("/")) filename = "/" + filename;
    
    if(LittleFS.exists(filename) && filename.endsWith(".csv")){
      LittleFS.remove(filename);
      Serial.printf("Deleted playlist: %s\n", filename.c_str());
      
      if(currentPlaylistFile == filename){
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
          String name = String(file.name());
          if(name.endsWith(".csv")){
            currentPlaylistFile = name;
            loadPlaylistMetadata(name);
            if(streamCount > 0){
              selectStream(0);
            }
            break;
          }
          file = root.openNextFile();
        }
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleVisualReset() {
    Preferences prefs;
    prefs.begin("radio", false);

    // === Сбрасываем только визуальные настройки ===
    const char* keys[] = {
      // Координаты текста
      "stationX","stationY",
      "title1X","title1Y",
      "title2X","title2Y",
      "bitrateX","bitrateY",
      "clockY",
      "volumeX","volumeY",

      // Цвета текста и стрелок
      "colorStation","colorTitle","colorBitrate","colorVolume",
      "colorClock","colorNeedleMain","colorNeedleRed",

      // Позиции стрелок
      "needleLX","needleLY",
      "needleRX","needleRY",

      // Вид стрелок
      "needleThk","needleMain","needleRed","needleCY",

      // Углы стрелок
      "needleMinAngle","needleMaxAngle",

      // Скорость стрелок
      "needleUpSpeed","needleDownSpeed"
    };

    for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        prefs.remove(keys[i]);
    }

    prefs.end();

    server.send(200, "text/plain", "Visual settings reset. Rebooting...");
    delay(300);
    ESP.restart();
}

// === Web Handlers ===
void handleRoot() {
  // Если мы в режиме AP (нет интернета), показываем страницу настройки WiFi
  if (WiFi.getMode() == WIFI_AP) {
    server.send(200, "text/html", getWiFiSetupHTML());
  } else {
    // Иначе показываем обычный интерфейс
    server.send(200, "text/html", getWebInterfaceHTML());
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404 - Not Found");
}

void handleNext() {
  nextStream();
  server.send(200, "text/plain", "OK");
}

void handlePrev() {
  prevStream();
  server.send(200, "text/plain", "OK");
}

void handlePlay() {
  pendingToggle = true;
  Serial.println("Requested: toggle play/pause");
  server.send(200, "text/plain", "OK");
}

void handlePlayURL() {
  if(server.hasArg("url")){
    String url = server.arg("url");
    if(url.startsWith("http://") || url.startsWith("https://")){
      audio.setPinout(AUDIO_I2S_BCLK, AUDIO_I2S_LRCLK, AUDIO_I2S_DOUT);
      audio.connecttohost(url.c_str());
      audio.setVolume(currentVolume);
      radioStarted = true;
      isPaused = false;
      currentStreamName = "Direct URL";
      currentStreamURL = url;
      redrawScreen();
      Serial.printf("Playing URL: %s\n", url.c_str());
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSetVolume() {
  if(server.hasArg("v")){
    currentVolume = server.arg("v").toInt();
    currentVolume = constrain(currentVolume, 0, 21);
    audio.setVolume(currentVolume);
    drawVolumeDisplay();
    saveSettings();
    Serial.printf("Volume set to %d\n", currentVolume);
  }
  server.send(200, "text/plain", "OK");
}

void handleSetNeedle() {
  if(server.hasArg("lx")) needlePosLX = server.arg("lx").toInt();
  if(server.hasArg("ly")) needlePosLY = server.arg("ly").toInt();
  if(server.hasArg("rx")) needlePosRX = server.arg("rx").toInt();
  if(server.hasArg("ry")) needlePosRY = server.arg("ry").toInt();
  
  saveSettings();
  Serial.printf("Needle positions - L(%d,%d) R(%d,%d)\n", 
                needlePosLX, needlePosLY, needlePosRX, needlePosRY);
  server.send(200, "text/plain", "OK");
}

void handleSetNeedleAppearance() {
  if(server.hasArg("thk")) needleThickness = server.arg("thk").toInt();
  if(server.hasArg("main")) needleLenMain = server.arg("main").toInt();
  if(server.hasArg("red")) needleLenRed = server.arg("red").toInt();
  if(server.hasArg("cy")) needleCY = server.arg("cy").toInt();
  
  saveSettings();
  Serial.printf("Needle appearance - Thickness:%d, Main:%d, Red:%d, CY:%d\n", 
                needleThickness, needleLenMain, needleLenRed, needleCY);
  server.send(200, "text/plain", "OK");
}

void handleSetTextPos() {
  if(server.hasArg("stationX")) { stationNameX = server.arg("stationX").toInt(); saveSettings(); }
  if(server.hasArg("stationY")) { stationNameY = server.arg("stationY").toInt(); saveSettings(); redrawScreen(); }
  if(server.hasArg("title1X")) { streamTitle1X = server.arg("title1X").toInt(); saveSettings(); }
  if(server.hasArg("title1Y")) { streamTitle1Y = server.arg("title1Y").toInt(); saveSettings(); redrawScreen(); }
  if(server.hasArg("title2X")) { streamTitle2X = server.arg("title2X").toInt(); saveSettings(); }
  if(server.hasArg("title2Y")) { streamTitle2Y = server.arg("title2Y").toInt(); saveSettings(); redrawScreen(); }
  if(server.hasArg("bitrateX")) { bitrateX = server.arg("bitrateX").toInt(); saveSettings(); }
  if(server.hasArg("bitrateY")) { bitrateY = server.arg("bitrateY").toInt(); saveSettings(); redrawScreen(); }
  if(server.hasArg("clockY")) { clockY = server.arg("clockY").toInt(); saveSettings(); redrawClock = true; }
  if(server.hasArg("volumeX")) { volumeX = server.arg("volumeX").toInt(); saveSettings(); }
  if(server.hasArg("volumeY")) { volumeY = server.arg("volumeY").toInt(); saveSettings(); drawVolumeDisplay(); }
  
  server.send(200, "text/plain", "OK");
}

void handleSelectStream() {
  if(server.hasArg("idx")){
    int idx = server.arg("idx").toInt();
    selectStream(idx);
  }
  server.send(200, "text/plain", "OK");
}

void handleSelectPlaylist() {
  if(server.hasArg("pl")){
    String filename = server.arg("pl");
    loadPlaylistMetadata(filename);
    if(streamCount > 0){
      selectStream(0);
      Serial.printf("Playlist changed to: %s\n", filename.c_str());
    }
    saveSettings();
  }
  server.send(200, "text/plain", "OK");
}

void handleSelectBg() {
  if(server.hasArg("bg")){
    String newBg = server.arg("bg");
    if(!newBg.startsWith("/")) newBg = "/" + newBg;
    
    if(LittleFS.exists(newBg)){
      currentBackground = newBg;
      loadBackground();
      redrawClock = true;
      redrawScreen();
      saveSettings();
      Serial.printf("Background changed to: %s\n", newBg.c_str());
    }
  }
  server.send(200, "text/plain", "OK");
}

// === API Handlers ===
void handleAPIPlayer() {
  server.send(200, "application/json", getPlayerStatus());
}

void handleAPIStreams() {
  server.send(200, "application/json", getStreamsList());
}

void handleAPINetwork() {
  server.send(200, "application/json", getNetworkInfo());
}

// === Upload Handlers ===
void handleUploadComplete() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Upload complete");
}

void handleUploadProcess() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  static size_t totalWritten = 0;
  
  if(upload.status == UPLOAD_FILE_START){
    String filename = "/" + upload.filename;
    totalWritten = 0;
    
    if(!filename.endsWith(".jpg") && !filename.endsWith(".jpeg") && !filename.endsWith(".csv")){
      Serial.printf("Invalid file type: %s\n", filename.c_str());
      return;
    }
    
    Serial.printf("Upload start: %s\n", filename.c_str());
    uploadFile = LittleFS.open(filename, "w", true);
    
    if(!uploadFile){
      Serial.printf("Failed to open file for writing: %s\n", filename.c_str());
    } else {
      Serial.println("File opened successfully (buffered mode)");
    }
  } 
  else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile){
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      totalWritten += written;
      
      if(totalWritten % 5120 < upload.currentSize){
        Serial.printf("Progress: %u bytes written\n", totalWritten);
      }
    } else {
      Serial.println("❌ File not open for writing!");
    }
  } 
  else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile){
      uploadFile.close();
      Serial.printf("Upload complete: %s (%u bytes total)\n", 
                    upload.filename.c_str(), upload.totalSize);
      
      String filename = "/" + upload.filename;
      if(filename.endsWith(".jpg") || filename.endsWith(".jpeg")){
        currentBackground = filename;
        loadBackground();
        redrawScreen();
        saveSettings();
        Serial.println("Background automatically loaded");
      }
      else if(filename.endsWith(".csv")){
        loadPlaylistMetadata(filename);
        if(streamCount > 0){
          selectStream(0);
        }
        Serial.println("Playlist automatically loaded");
      }
    } else {
      Serial.println("Upload failed - file was not open");
    }
    totalWritten = 0;
  }
  else if(upload.status == UPLOAD_FILE_ABORTED){
    if(uploadFile){
      uploadFile.close();
    }
    Serial.println("Upload aborted");
    totalWritten = 0;
  }
}

void handleSetColors() {
  if(server.hasArg("station")) colorStation = strtol(server.arg("station").c_str(), NULL, 16);
  if(server.hasArg("title")) colorTitle = strtol(server.arg("title").c_str(), NULL, 16);
  if(server.hasArg("bitrate")) colorBitrate = strtol(server.arg("bitrate").c_str(), NULL, 16);
  if(server.hasArg("volume")) colorVolume = strtol(server.arg("volume").c_str(), NULL, 16);
  if(server.hasArg("clock")) colorClock = strtol(server.arg("clock").c_str(), NULL, 16);
  if(server.hasArg("needleMain")) colorNeedleMain = strtol(server.arg("needleMain").c_str(), NULL, 16);
  if(server.hasArg("needleRed")) colorNeedleRed = strtol(server.arg("needleRed").c_str(), NULL, 16);
  
  saveSettings();
  redrawScreen();
  server.send(200, "text/plain", "OK");
}

void handleUpdateTFT() {
  redrawScreen();
  server.send(200, "text/plain", "TFT Updated");
}

void handleAPIColors() {
  String json = "{";
  json += "\"station\":" + String(colorStation) + ",";
  json += "\"title\":" + String(colorTitle) + ",";
  json += "\"bitrate\":" + String(colorBitrate) + ",";
  json += "\"volume\":" + String(colorVolume) + ",";
  json += "\"clock\":" + String(colorClock) + ",";
  json += "\"needleMain\":" + String(colorNeedleMain) + ",";
  json += "\"needleRed\":" + String(colorNeedleRed);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetNeedleAngles() {
  if(server.hasArg("minAngle")) needleMinAngle = server.arg("minAngle").toInt();
  if(server.hasArg("maxAngle")) needleMaxAngle = server.arg("maxAngle").toInt();
  
  // Ограничиваем значения
  needleMinAngle = constrain(needleMinAngle, -360, 0);
  needleMaxAngle = constrain(needleMaxAngle, -360, 0);
  
  saveSettings();
  server.send(200, "text/plain", "OK");
}

void handleSetNeedleSpeeds() {
  if(server.hasArg("upSpeed")) needleUpSpeed = server.arg("upSpeed").toInt();
  if(server.hasArg("downSpeed")) needleDownSpeed = server.arg("downSpeed").toInt();
  
  // Ограничиваем значения
  needleUpSpeed = constrain(needleUpSpeed, 1, 20);
  needleDownSpeed = constrain(needleDownSpeed, 1, 10);
  
  saveSettings();
  server.send(200, "text/plain", "OK");
}

// -----------------------------
// File / Playlist Web Handlers
// -----------------------------

// Download file (force download)
void handleDownload() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file parameter");
    return;
  }
  String filename = server.arg("file");
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!LittleFS.exists(filename)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File f = LittleFS.open(filename, "r");
  String contentType = "application/octet-stream";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (filename.endsWith(".csv")) contentType = "text/csv";

  // Force download filename
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename.substring(1) + String("\""));
  server.streamFile(f, contentType);
  f.close();
}

// Return JSON list of playlists (.csv)
void handleAPIPlaylists() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  while(file) {
    String name = String(file.name());
    if(name.endsWith(".csv")) {
      if(!first) json += ",";
      json += "{\"name\":\"" + name + "\"}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Return JSON list of backgrounds (.jpg)
void handleAPIBackgrounds() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  while(file) {
    String name = String(file.name());
    if(name.endsWith(".jpg") || name.endsWith(".jpeg")) {
      if(!first) json += ",";
      json += "{\"name\":\"" + name + "\"}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Return content of playlist as JSON array [{name, url}, ...]
// GET /viewplaylist?pl=filename.csv
void handleViewPlaylist() {
  if (!server.hasArg("pl")) {
    server.send(400, "text/plain", "Missing filename");
    return;
  }
  String filename = server.arg("pl");
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!LittleFS.exists(filename)) {
    server.send(404, "text/plain", "Playlist not found");
    return;
  }

  File f = LittleFS.open(filename, "r");
  String json = "[";
  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    if (tab1 == -1 || tab2 == -1) continue;

    String name = line.substring(0, tab1);
    String url = line.substring(tab1 + 1, tab2);
    if (!first) json += ",";
    json += "{\"name\":\"" + name + "\",\"url\":\"" + url + "\"}";
    first = false;
  }
  f.close();
  json += "]";
  server.send(200, "application/json", json);
}

// Save playlist: POST /saveplaylist?pl=filename.csv with JSON body = [{name,url},...]
void handleSavePlaylist() {
  if (!server.hasArg("pl")) {
    server.send(400, "text/plain", "Missing filename");
    return;
  }
  String filename = server.arg("pl");
  if (!filename.startsWith("/")) filename = "/" + filename;

  String body = server.arg("plain"); // body contains JSON
  if (body.length() == 0) {
    server.send(400, "text/plain", "Missing body");
    return;
  }

  // Parse JSON
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "text/plain", String("JSON parse error: ") + err.c_str());
    return;
  }

  // Write into file: each row -> name \t url \t 0\n
  File f = LittleFS.open(filename, "w");
  if (!f) {
    server.send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  for (JsonVariant v : doc.as<JsonArray>()) {
    String name = v["name"] | "";
    String url = v["url"] | "";
    // Basic sanitize: replace newlines/tabs
    name.replace("\n"," ");
    name.replace("\r"," ");
    name.replace("\t"," ");
    url.replace("\n"," ");
    url.replace("\r"," ");
    url.replace("\t"," ");
    f.printf("%s\t%s\t0\n", name.c_str(), url.c_str());
  }
  f.close();

  // Refresh playlist metadata in RAM if your code uses it
  loadPlaylistMetadata(filename);

  server.send(200, "text/plain", "Saved");
}

// === Web Server Init ===
void initWebServer() {
  // Setup endpoints
  server.on("/", handleRoot);
  server.on("/setup", handleWiFiSetupPage);
  server.on("/scan", handleWiFiScan);
  server.on("/savewifi", handleWiFiSave);

  server.on("/setcolors", handleSetColors);
  server.on("/updatetft", handleUpdateTFT);
  server.on("/visualreset", handleVisualReset);


  server.on("/download", handleDownload);
  server.on("/viewplaylist", handleViewPlaylist);
  server.on("/saveplaylist", HTTP_POST, handleSavePlaylist);

  
  // Other endpoints
  server.on("/setneedlespeeds", handleSetNeedleSpeeds);
  server.on("/setneedleangles", handleSetNeedleAngles);
  server.on("/api/colors", handleAPIColors);
  server.on("/next", handleNext);
  server.on("/prev", handlePrev);
  server.on("/play", handlePlay);
  server.on("/playurl", handlePlayURL);
  server.on("/setvolume", handleSetVolume);
  server.on("/setneedle", handleSetNeedle);
  server.on("/setneedleapp", handleSetNeedleAppearance);
  server.on("/settextpos", handleSetTextPos);
  server.on("/selectstream", handleSelectStream);
  server.on("/selectplaylist", handleSelectPlaylist);
  server.on("/selectbg", handleSelectBg);
  server.on("/deletebg", handleDeleteBg);
  server.on("/deleteplaylist", handleDeletePlaylist);
  
  // API endpoints
  server.on("/api/player", handleAPIPlayer);
  server.on("/api/streams", handleAPIStreams);
  server.on("/api/backgrounds", handleAPIBackgrounds);
  server.on("/api/playlists", handleAPIPlaylists);
  server.on("/api/network", handleAPINetwork);
  server.on("/upload", HTTP_POST, 
    handleUploadComplete,
    handleUploadProcess
  );
  
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started!");
  Serial.print("Open: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":80");
}

// === Setup ===
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  if (psramInit()) {
    Serial.printf("PSRAM: OK, size: %d bytes\n", ESP.getPsramSize());
  } else {
    Serial.println("ERROR: PSRAM not initialized!");
  }
  
  xMutex = xSemaphoreCreateMutex();
  
  // === TFT Display ===
  gfx->begin();
  gfx->fillRect(0, 0, gfx->width(), gfx->height(), RGB565_WHITE);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BRIGHTNESS);

  // === File System ===
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS mount failed - trying to format...");
    if (LittleFS.begin(true)) {
      Serial.println("✅ LittleFS formatted and mounted successfully!");
    } else {
      Serial.println("❌ LittleFS format failed!");
      gfx->fillScreen(RGB565_RED);
      gfx->setFont(&FreeSerifBoldItalic14pt8b);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setCursor(20, 100);
      gfx->print("FS ERROR!");
      gfx->setCursor(20, 130);
      gfx->print("Check partition");
      while (true) delay(1000);
    }
  }
  Serial.println("LittleFS mounted");
  Serial.printf("Total space: %d bytes\n", LittleFS.totalBytes());
  Serial.printf("Used space: %d bytes\n", LittleFS.usedBytes());
  Serial.printf("Free space: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());

  // === Load Settings ===
  loadSettings();

  // === Buttons ===
  pinMode(BTN_VOL_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  pinMode(BTN_PREV_PIN, INPUT_PULLUP);
  pinMode(BTN_PLAY_PAUSE_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_VOL_UP_PIN), volUpISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_VOL_DOWN_PIN), volDownISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_NEXT_PIN), nextStreamISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_PREV_PIN), prevStreamISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_PLAY_PAUSE_PIN), playPauseISR, FALLING);

  // === JPEG Decoder ===
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tpgOutputToSprite);

  // === Sprites ===
  createSprites();
  loadBackground();  // Теперь безопасно - создаст черный фон если файла нет
  
  // === WiFi ===
  bool wifi_ok = connectToWiFi();
  
  // === Playlist ===
  loadPlaylistMetadata(currentPlaylistFile);  // Теперь безопасно - создаст пустой плейлист
  
  if (streamCount == 0) {
    Serial.println("⚠️ No streams in playlist. Add playlist.csv via web interface!");
    gfx->setFont(&FreeSerifBoldItalic14pt8b);
    gfx->setTextColor(RGB565_YELLOW);
    gfx->setCursor(10, 100);
    gfx->print("No playlist found");
    gfx->setCursor(10, 130);
    gfx->print("Upload playlist.csv");
    gfx->setCursor(10, 160);
    gfx->print("via web interface");
  }
  
  // === Audio ===
  audio.setPinout(AUDIO_I2S_BCLK, AUDIO_I2S_LRCLK, AUDIO_I2S_DOUT);
  Serial.printf("Before setInBufferSize: %d KB\n", audio.getInBufferSize() / 1024);
  audio.setInBufferSize(AUDIO_BUFFER_SIZE);
  Serial.printf("After setInBufferSize: %d KB\n", audio.getInBufferSize() / 1024);
  audio.setTone(0, 0, 0);
  audio.setVolume(currentVolume);
  Audio::audio_info_callback = myAudioInfo;
  
  // === Clock ===
  initClock();
  
  // === Web Server ===
  initWebServer();

  // === Display Task ===
  xTaskCreatePinnedToCore(displayTask, "Display Task", 4096, NULL, 2, NULL, 1);
  
  // === Auto-start last station (только если есть плейлист и WiFi) ===
  if(streamCount > 0 && wifiConnected){
    startRadio();
  } else {
    Serial.println("⚠️ Radio not started: no playlist or WiFi");
  }
  
  Serial.println("Setup completed!\n");
  drawVolumeDisplay();
  
  if (streamCount > 0 && wifiConnected) {
    showAction("System Ready");
  } else if (!wifiConnected) {
    showAction("WiFi Setup Mode", RGB565_ORANGE);
  } else {
    showAction("Upload Playlist", RGB565_YELLOW);
  }
}

// === Main Loop ===
// === Добавьте в главный цикл проверку таймаута метаданных ===
void loop() {
  server.handleClient();
  
  // Проверка таймаута метаданных
  if (metadataPending && (millis() - metadataRequestTime > METADATA_TIMEOUT)) {
    Serial.println("⚠️ Metadata timeout - proceeding without metadata");
    metadataPending = false;
    redrawScreen(); // Перерисовка без индикатора загрузки
  }
  
  // === Deferred radio control ===
  if (pendingToggle) {
    pendingToggle = false;
    
    if (radioStarted && !isPaused) {
      audio.pauseResume();
      isPaused = true;
      Serial.println("Radio paused");
    } else if (radioStarted && isPaused) {
      audio.pauseResume();
      isPaused = false;
      Serial.println("Radio resumed");
    } else {
      startRadio();
    }
    
    redrawScreen();
  }
  
  static unsigned long lastDiag = 0;
  if (millis() - lastDiag > 10000) {
    lastDiag = millis();
    
    Serial.printf("=== DIAGNOSTICS ===\n");
    Serial.printf("Radio: %s, Paused: %s\n", 
                  radioStarted ? "ON" : "OFF", 
                  isPaused ? "YES" : "NO");
    Serial.printf("InBuffer filled: %d KB / %d KB\n", 
      audio.inBufferFilled() / 1024,
      audio.getInBufferSize() / 1024);
    Serial.printf("Free heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
    Serial.printf("WiFi signal: %d dBm\n", WiFi.RSSI());
    if (radioStarted && !isPaused) {
      Serial.printf("Bitrate: %d kbps\n", audio.getBitRate() / 1000);
    }
    Serial.printf("Streams in playlist: %d\n", streamCount);
    Serial.printf("LittleFS free: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
  }
  
  if (radioStarted && !isPaused) {
    audio.loop();
    delay(1);
  }

  // === Physical buttons handling ===
  if (flagVolUp) {
    flagVolUp = false;
    if (currentVolume < 21) {
      currentVolume++;
      audio.setVolume(currentVolume);
      Serial.printf("Volume up: %d\n", currentVolume);
      drawVolumeDisplay();
      saveSettings();
    }
  }

  if (flagVolDown) {
    flagVolDown = false;
    if (currentVolume > 0) {
      currentVolume--;
      audio.setVolume(currentVolume);
      Serial.printf("Volume down: %d\n", currentVolume);
      drawVolumeDisplay();
      saveSettings();
    }
  }

  if (flagNext) {
    flagNext = false;
    nextStream();
    Serial.println("Next station");
    showAction("Next Station", RGB565_CYAN);
  }

  if (flagPrev) {
    flagPrev = false;
    prevStream();
    Serial.println("Previous station");
    showAction("Previous Station", RGB565_CYAN);
  }

  if (flagPlayPause) {
    flagPlayPause = false;
    pendingToggle = true;
    Serial.println("Toggle play/pause");
  }
  
  checkStreamChange();
  clearActionIfTimeout();
  delay(10);
}
