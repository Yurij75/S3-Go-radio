#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Audio.h>
#include <TJpg_Decoder.h>
#include <Preferences.h>
#include "time.h"
#include <LittleFS.h>
#include "config.h"
#include "led_music.h"
#include "ir_remote.h"
#include "backlight_control.h"
#include "display_text_utils.h"
#include "display_manager.h"
#include "psram_canvas.h"
#include "screens/screen_manager.h"
#include "screens/screen_settings.h"
#include "screens/screens.h"
#include "web_handlers.h"
#include "web_interface.h"
#include "web_ota.h"
#include "wifi_setup.h"
#include "volume_control.h"
#include <ArduinoJson.h>
#include <driver/gpio.h>

#include "sleep_timer.h"
#include "screensaver.h"
#include "yoEncoder.h"
#include "OneButton/OneButton.h"

#include <algorithm> // Добавьте эту строку

// === Preferences для сохранения настроек ===
#ifndef DISPLAY_PROFILE_NEEDLE_LEFT_MIRRORED
#define DISPLAY_PROFILE_NEEDLE_LEFT_MIRRORED 0
#endif
#ifndef DISPLAY_PROFILE_NEEDLE_ROTATION
#define DISPLAY_PROFILE_NEEDLE_ROTATION 0
#endif

// === LED Music ===
#ifdef WS2812B_PIN
LedMusic ledMusic;
#endif
bool ledMusicEnabled = false;
bool vuMeterEnabled = true;
bool primaryMarqueeEnabled = true;
bool secondaryMarqueeEnabled = false;
bool quoteMarqueeEnabled = false;
int ledStripCount = WS2812B_LED_COUNT;
int ledEffectIndex = 0;
String ledColorOrder = LED_COLOR_ORDER;
int eqBalance = 0;
int eqTreble = 0;
int eqMid = 0;
int eqBass = 0;
Preferences prefs;
static constexpr const char* PREF_KEY_PRIMARY_MARQUEE = "primMq";
static constexpr const char* PREF_KEY_SECONDARY_MARQUEE = "secMq";
static constexpr const char* PREF_KEY_SHOW_STATION_NUM = "showStNum";
static constexpr const char* PREF_KEY_STATION_NUM_FMT = "stNumFmt";
static constexpr const char* PREF_KEY_STATION_FONT_SIZE = "stFontSize";
static constexpr const char* kBootImagePath = "/boot.jpg";

static void warmupGpioIsrService() {
  const esp_err_t err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[WARN] gpio_install_isr_service failed: %d\n", static_cast<int>(err));
  }
}

// === Station number display ===
bool showStationNumber = true;  // Показывать номер станции
String stationNumberFormat = "{cur}";  // Только текущий номер
int stationTextFontSize = 1;  // 0-small, 1-normal, 2-large

// === Переменные для метаданных потока ===
String icy_station = "";
String icy_streamtitle = "";
String icy_url = "";
String icy_bitrate = "";
String id3_data = "";
String audio_codec = "";
String audio_frequency = "";
String audio_bitness = "";

// === Text Colors ===
uint16_t colorStation = DISPLAY_PROFILE_COLOR_STATION;
uint16_t colorTitle = DISPLAY_PROFILE_COLOR_TITLE; 
uint16_t colorBitrate = DISPLAY_PROFILE_COLOR_BITRATE;
uint16_t colorVolume = DISPLAY_PROFILE_COLOR_VOLUME;
uint16_t colorClock = DISPLAY_PROFILE_COLOR_CLOCK;
uint16_t colorNeedleMain = DISPLAY_PROFILE_COLOR_NEEDLE_MAIN;
uint16_t colorNeedleRed = DISPLAY_PROFILE_COLOR_NEEDLE_RED;
uint16_t colorIP = DISPLAY_PROFILE_COLOR_IP;

// --- TFT Display - Auto-select based on config.h ---
int tftRotation = DISPLAY_PROFILE_ROTATION; // динамический поворот экрана
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);

#if defined(DISPLAY_ST7796)
  Arduino_GFX *gfx = new Arduino_ST7796(bus, TFT_RST, TFT_ROTATION);

#elif defined(DISPLAY_ILI9341)
  Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, TFT_ROTATION);

#elif defined(DISPLAY_ILI9488)
  Arduino_GFX *gfx = new Arduino_ILI9488(bus, TFT_RST, TFT_ROTATION);  

#elif defined(DISPLAY_NV3007)
  Arduino_GFX *gfx = new Arduino_NV3007(bus, TFT_RST, 1, false, 148, 428, 10, 0, 10, 0);

#elif defined(DISPLAY_ST7735_160x128)
  // Для ST7735 160x128 пикселей
  // Конструктор: bus, rst, rotation, IPS, width, height, col_offset1, row_offset1, col_offset2, row_offset2
  Arduino_GFX *gfx = new Arduino_ST7735(bus, TFT_RST, TFT_ROTATION, false, 160, 128, 0, 0, 0, 0);

#elif defined(DISPLAY_ST7789)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true);

#elif defined(DISPLAY_ST7789_172)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 172, 320, 34, 0, 34, 0);

#elif defined(DISPLAY_ST7789_76)
  Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, false, 76, 284, 82, 18, 82, 0); // false надо

#else
  #error "No display type defined! Please uncomment one DISPLAY_* in config.h"
#endif

Arduino_Canvas *bgSprite;
Arduino_Canvas *needleSpriteL;
Arduino_Canvas *needleSpriteR;
Arduino_Canvas *clockSprite;
Arduino_Canvas *analogClockSprite;
int clockW, clockH;
int analogClockW, analogClockH;
int clockSpriteX, clockSpriteY;

Audio audio;
WebServer server(80);

// --- Global variables ---
int vuWidth, vuHeight;
bool wifiConnected = false, radioStarted = false;
bool isPaused = false;
SemaphoreHandle_t xMutex;

// В глобальные переменные
volatile bool pendingScreenChange = false;
volatile bool pendingStreamChange = false;
volatile bool pendingPrevStream = false;
// Pending selection (debounced): index scheduled for selection and request time
volatile int pendingSelectedStreamIndex = -1;
volatile unsigned long pendingSelectRequestTime = 0;
// === Sleep Timer ===
SleepTimer sleepTimer;
volatile bool flagSleepTimer = false;
volatile unsigned long lastSleepTimerISR = 0;

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
int needlePosLX = DISPLAY_PROFILE_NEEDLE_L_X;
int needlePosLY = DISPLAY_PROFILE_NEEDLE_L_Y;
int needlePosRX = DISPLAY_PROFILE_NEEDLE_R_X;
int needlePosRY = DISPLAY_PROFILE_NEEDLE_R_Y;

// === Needle Animation ===
int needleUpSpeed = DISPLAY_PROFILE_NEEDLE_UP_SPEED;
int needleDownSpeed = DISPLAY_PROFILE_NEEDLE_DOWN_SPEED;

// --- Needle settings ---
int needleThickness = DISPLAY_PROFILE_NEEDLE_THICKNESS;
int needleLenMain = DISPLAY_PROFILE_NEEDLE_MAIN_LEN;
int needleLenRed = DISPLAY_PROFILE_NEEDLE_RED_LEN;
int needleCY = DISPLAY_PROFILE_NEEDLE_CENTER_Y;
bool debugBordersEnabled = DEBUG_BORDERS;
bool leftNeedleMirrored = DISPLAY_PROFILE_NEEDLE_LEFT_MIRRORED != 0;
int needleRotation = DISPLAY_PROFILE_NEEDLE_ROTATION;
int needleSpriteWidth = SPRITE_WIDTH;
int needleSpriteHeight = SPRITE_HEIGHT;

// === Needle Angles ===
int needleMinAngle = DISPLAY_PROFILE_NEEDLE_MIN_ANGLE;
int needleMaxAngle = DISPLAY_PROFILE_NEEDLE_MAX_ANGLE;

// --- Volume ---
int currentVolume = AUDIO_DEFAULT_VOLUME;
int currentStationOvol = 0;

// --- Clock ---
String prevClock = "--:--:--";
String prevBitrate = "";
bool redrawClock = true;
String prevStreamName = "";

// === Station list overlay ===
bool showStationList = false;
enum class StationListMode {
  Playlists,
  Stations
};
StationListMode stationListMode = StationListMode::Playlists;
int stationListSelectedIndex = 0;
int stationListPage = 0;
std::vector<String> stationListPlaylists;

// --- Background filename ---
String currentBackground = DISPLAY_PROFILE_DEFAULT_BACKGROUND;

// === Text positions (настраиваемые координаты) ===
int stationNameX = DISPLAY_PROFILE_STATION_X, stationNameY = DISPLAY_PROFILE_STATION_Y;
int streamTitle1X = DISPLAY_PROFILE_TITLE1_X, streamTitle1Y = DISPLAY_PROFILE_TITLE1_Y;
int streamTitle2X = DISPLAY_PROFILE_TITLE2_X, streamTitle2Y = DISPLAY_PROFILE_TITLE2_Y;
int stationScrollWidth = 0;
int title1ScrollWidth = 0;
int title2ScrollWidth = 0;
int marqueePauseMs = 1500;
int bitrateX = DISPLAY_PROFILE_BITRATE_X, bitrateY = DISPLAY_PROFILE_BITRATE_Y;
int ipX = DISPLAY_PROFILE_IP_X, ipY = DISPLAY_PROFILE_IP_Y;
int clockX = DISPLAY_PROFILE_CLOCK_X, clockY = DISPLAY_PROFILE_CLOCK_Y;
int volumeX = DISPLAY_PROFILE_VOLUME_X, volumeY = DISPLAY_PROFILE_VOLUME_Y;
int quoteX = 0, quoteY = 0, quoteW = 320;
String quoteApiUrl = "";
int vuBarsHeight = 34;
int vuBarsSegments = 15;
int vuBarsOffsetX = 0;
int vuBarsOffsetY = 0;
uint16_t vuBarsColorBottom = RGB565_LIME;
uint16_t vuBarsColorTop = RGB565_RED;
int vuCardioX = 8;
int vuCardioY = 92;
int vuCardioW = 304;
int vuCardioH = 104;
int fftWindowX = 0;
int fftWindowY = 100;
int fftWindowW = 320;
int fftWindowH = 100;
int fftBands = 32;
int fftSegments = 46;
int fftMinBin = 0;
int fftMaxBin = FFT_BANDS - 1;
int fftAggregation = 0;
int fftCurve = 1;
int fftProfile = 0;
float fftGain = 1.5f;
float fftPreGain = 1.0f;
float fftNoiseGate = 0.0f;
float fftNoiseFloor = 10.0f;
float fftSensitivity = 1.5f;
float fftGamma = 0.72f;
float fftLogStrength = 4.0f;
float fftAttack = 0.75f;
float fftRelease = 0.35f;
float fftLowBoost = 1.15f;
float fftMidBoost = 1.3f;
float fftHighBoost = 1.25f;
int fftBarGap = 2;
int fftSideMargin = 4;
int fftTopMargin = 8;
int fftBottomMargin = 12;
int fftMinLitSegments = 0;
bool fftPeakHold = true;
int fftPeakFall = 2;
bool fftMirror = false;
bool fftInvert = false;
// Инверсия всего дисплея (негатив/позитив)
bool displayInverted = false;
uint16_t fftColorBottom = RGB565_WHITE;
uint16_t fftColorTop = RGB565_BLUE;
uint16_t fftColorPeak = RGB565_WHITE;
uint16_t fftColorOff = RGB565(0, 28, 0);

// === LAZY PLAYLIST LOADING ===
struct PlaylistEntry {
  String name;
};

std::vector<PlaylistEntry> playlistIndex;  // Динамический вектор вместо массива
int streamCount = 0;
String currentPlaylistFile = "/playlist.csv";
String currentPlayingPlaylistFile = "/playlist.csv";
String currentStreamURL = "";
String currentStreamName = "";
int currentStreamIndex = 0;
volatile bool streamChangeRequested = false;
unsigned long lastStreamChangeMillis = 0;

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
static bool lastLedToggleButtonState = HIGH;
static bool lastLedEffectButtonState = HIGH;
static unsigned long lastLedToggleButtonAt = 0;
static unsigned long lastLedEffectButtonAt = 0;

const unsigned long DEBOUNCE_MS = 100; // Уменьшили для кнопок

// === Rotary Encoder ===
yoEncoder* encoder = nullptr;
yoEncoder* navEncoder = nullptr;
// OneButton instances for encoder buttons
OneButton* encoderBtn = nullptr;
OneButton* navEncoderBtn = nullptr;
volatile unsigned long lastEncoderRotationAt = 0;
volatile unsigned long lastNavEncoderRotationAt = 0;
// Переменные состояния кнопок энкодеров
volatile unsigned long lastEncoderBtnPress = 0;
volatile unsigned long encoderBtnPressStart = 0;
volatile bool encoderBtnPressed = false;
volatile unsigned long lastNavEncoderBtnPress = 0;
volatile unsigned long navEncoderBtnPressStart = 0;
volatile bool navEncoderBtnPressed = false;

// Тайминги для распознавания кликов/длинных нажатий
const unsigned long ENCODER_BTN_DEBOUNCE = 50;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long CLICK_TIMEOUT = 400;

// Флаги для обработки в основном цикле
bool pendingEncoderClick = false;
bool pendingEncoderLongPress = false;
int pendingEncoderClicks = 0;
bool pendingNavEncoderClick = false;
bool pendingNavEncoderLongPress = false;
unsigned long lastClickTime = 0; // Добавьте для подсчета кликов

// Вращение энкодера
volatile long lastEncoderValue = 0;
volatile long currentEncoderValue = 0;
volatile long lastNavEncoderValue = 0;
bool displaySettingsSavePending = false;
bool streamStateSavePending = false;
bool audioInfoRefreshPending = false;
unsigned long streamStateSaveAt = 0;
bool volumeSavePending = false;
unsigned long volumeSaveAt = 0;
const unsigned long STREAM_STATE_SAVE_DELAY_MS = 3000;
const unsigned long VOLUME_SAVE_DELAY_MS = 1500;
const unsigned long ENCODER_BUTTON_ROTATION_GUARD_MS = 150;
const unsigned long ENCODER_MIN_VALID_PRESS_MS = 30;
constexpr const char* PREF_KEY_SCREENSAVER_ENABLED = "ssEnabled";
constexpr const char* PREF_KEY_SCREENSAVER_IDLE = "ssIdleSec";
constexpr bool kHasVolumeEncoder = (ENCODER_A_PIN != 255 && ENCODER_B_PIN != 255);
constexpr bool kHasNavEncoder = (NAV_ENCODER_A_PIN != 255 && NAV_ENCODER_B_PIN != 255);
constexpr bool kHasNavEncoderButton = (NAV_ENCODER_BTN_PIN != 255);
bool screensaverEnabled = false;
int screensaverIdleSeconds = 60;
unsigned long lastUserActivityMs = 0;
CityScreensaver* cityScreensaver = nullptr;

// === Forward Declarations ===
void redrawScreen();
void loadBackground();
void startRadio();
void savePlaybackState();
void saveRuntimeSettings();
void saveDisplaySettings();
void saveSettings();
void scheduleDisplaySettingsSave();
void flushPendingDisplaySettingsSave();
String makeScreenEnabledPrefKey(ScreenId id);
void scheduleStreamStateSave();
void servicePendingStreamStateSave();
void scheduleVolumeSave();
void servicePendingVolumeSave();
void loadSettings();
void applyAudioEq();
void applyCurrentVolume();
bool shouldShowMetadataScreen();
void loadPlaylistMetadata(String filename);
void selectStream(int idx);
void handleVolumeEncoderRotation();
void handleNavEncoderRotation();
void handleNavEncoderButton();
void handleLedControlButtons();
void handleAPIAudioInfo();
void noteUserActivity();
void serviceScreensaver();
bool isScreensaverActive();
void setScreensaverEnabled(bool enabled);
void setScreensaverIdleSeconds(int seconds);
// === Forward Declarations для таймера сна ===
void handleSleepTimer();
void handleAPISleepTimer();
void handleVolumeStep(int delta);
bool activatePlaylistByIndex(int idx);
bool selectNextPlaylist();
String getStreamName(int idx);
bool getStreamByIndexWithOvol(int idx, String& outName, String& outURL, int& outOvol);
void cycleToNextScreen();
void openPlaylistList();
void openStationList();
void closeStationList(bool redraw = true);
void drawStationList();
void moveStationListSelection(int delta);
void moveStationListPage(int delta);
void selectStationListItem();
void showCenteredSplash(const String& text, uint16_t color, unsigned long durationMs = 0);
void showStartupIpSplash();
bool drawBootImageBackground();

static bool jpgOutputToGfx(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!gfx) return false;
  gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return true;
}

bool drawBootImageBackground() {
  if (!gfx || !LittleFS.exists(kBootImagePath)) {
    return false;
  }

  File file = LittleFS.open(kBootImagePath, "r");
  if (!file) {
    return false;
  }

  const size_t fileSize = file.size();
  if (fileSize == 0) {
    file.close();
    return false;
  }

  uint8_t* jpegData = (uint8_t*)ps_malloc(fileSize);
  if (!jpegData) {
    file.close();
    return false;
  }

  file.read(jpegData, fileSize);
  file.close();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(jpgOutputToGfx);
  TJpgDec.drawJpg(0, 0, jpegData, fileSize);
  free(jpegData);
  return true;
}

bool isScreensaverActive() {
  return cityScreensaver && cityScreensaver->isEnabled();
}

void noteUserActivity() {
  lastUserActivityMs = millis();
  if (isScreensaverActive()) {
    cityScreensaver->disable();
    setDisplayTaskPaused(false);
    redrawScreen();
  }
}

void setScreensaverEnabled(bool enabled) {
  screensaverEnabled = enabled;
  if (screensaverEnabled && cityScreensaver) {
    cityScreensaver->begin(false);
  }
  if (!screensaverEnabled && isScreensaverActive()) {
    cityScreensaver->disable();
    setDisplayTaskPaused(false);
    redrawScreen();
  }
  noteUserActivity();
}

void setScreensaverIdleSeconds(int seconds) {
  screensaverIdleSeconds = constrain(seconds, 5, 3600);
  noteUserActivity();
}

void serviceScreensaver() {
  if (!cityScreensaver || !screensaverEnabled || sleepTimer.isSleepMode() || showStationList) return;

  const unsigned long idleMs = static_cast<unsigned long>(screensaverIdleSeconds) * 1000UL;
  const unsigned long now = millis();
  if (!cityScreensaver->isEnabled()) {
    if (idleMs == 0 || now - lastUserActivityMs < idleMs) return;
    setDisplayTaskPaused(true);
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
      cityScreensaver->enable();
      xSemaphoreGive(xMutex);
    }
    return;
  }

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    cityScreensaver->update();
    xSemaphoreGive(xMutex);
  }
}

int getStationListItemsPerPage() {
    const int lineHeight = 28;
    const int topPad = 34;
    const int bottomPad = 4;
    return std::max(1, (vuHeight - topPad - bottomPad) / lineHeight);
}

String normalizePlaylistFilename(String name) {
    if (!name.startsWith("/")) name = "/" + name;
    return name;
}

String getPlaylistDisplayName(String name) {
    name = normalizePlaylistFilename(name);
    if (name.startsWith("/")) name = name.substring(1);
    if (name.endsWith(".csv")) name = name.substring(0, name.length() - 4);
    return name;
}

void refreshStationListPlaylists() {
    stationListPlaylists.clear();

    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        String name = normalizePlaylistFilename(String(file.name()));
        if (name.endsWith(".csv")) {
            stationListPlaylists.push_back(name);
        }
        file = root.openNextFile();
    }

    std::sort(stationListPlaylists.begin(), stationListPlaylists.end());
}

int getStationListTotalItems() {
    if (stationListMode == StationListMode::Playlists) {
        return static_cast<int>(stationListPlaylists.size());
    }
    return streamCount + 1;  // first empty row returns to playlists
}

void clampStationListSelection() {
    const int totalItems = getStationListTotalItems();
    if (totalItems <= 0) {
        stationListSelectedIndex = 0;
        stationListPage = 0;
        return;
    }

    stationListSelectedIndex = constrain(stationListSelectedIndex, 0, totalItems - 1);
    const int itemsPerPage = getStationListItemsPerPage();
    const int maxPage = std::max(0, (totalItems - 1) / itemsPerPage);
    stationListPage = constrain(stationListPage, 0, maxPage);

    const int firstVisible = stationListPage * itemsPerPage;
    const int lastVisible = firstVisible + itemsPerPage - 1;
    if (stationListSelectedIndex < firstVisible) {
        stationListPage = stationListSelectedIndex / itemsPerPage;
    } else if (stationListSelectedIndex > lastVisible) {
        stationListPage = stationListSelectedIndex / itemsPerPage;
    }
}

void setStationListEncoderBounds() {
    if (!kHasNavEncoder && encoder) {
        encoder->setBoundaries(-100, 100, false);
        encoder->setEncoderValue(0);
        lastEncoderValue = 0;
    }
    if (navEncoder) {
        navEncoder->setEncoderValue(0);
        lastNavEncoderValue = 0;
    }
}

void openPlaylistList() {
    refreshStationListPlaylists();
    if (stationListPlaylists.empty()) return;

    setDisplayTaskPaused(true);
    showStationList = true;
    stationListMode = StationListMode::Playlists;
    stationListSelectedIndex = 0;
    String selectedPlaylist = normalizePlaylistFilename(currentPlaylistFile);
    for (int i = 0; i < static_cast<int>(stationListPlaylists.size()); ++i) {
        if (stationListPlaylists[i] == selectedPlaylist) {
            stationListSelectedIndex = i;
            break;
        }
    }
    stationListPage = stationListSelectedIndex / getStationListItemsPerPage();
    setStationListEncoderBounds();
    drawStationList();
}

void openStationList() {
    setDisplayTaskPaused(true);
    showStationList = true;
    stationListMode = StationListMode::Stations;
    stationListSelectedIndex = 0;
    if (streamCount > 0) {
        stationListSelectedIndex = (currentPlaylistFile == currentPlayingPlaylistFile)
                                       ? constrain(currentStreamIndex + 1, 1, streamCount)
                                       : 1;
    }
    stationListPage = stationListSelectedIndex / getStationListItemsPerPage();
    setStationListEncoderBounds();
    drawStationList();
}

void closeStationList(bool redraw) {
    if (!showStationList) return;
    showStationList = false;
    if (!kHasNavEncoder && encoder) {
        encoder->setBoundaries(kVolumePercentMin, kVolumePercentMax, false);
        encoder->setEncoderValue(currentVolume);
        lastEncoderValue = currentVolume;
    }
    if (navEncoder) {
        navEncoder->setEncoderValue(0);
        lastNavEncoderValue = 0;
    }
    setDisplayTaskPaused(false);
    if (redraw) {
        loadBackground();
        redrawScreen();
    }
}

void drawStationList() {
    if (!showStationList || !gfx || !bgSprite) return;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    clampStationListSelection();

    bgSprite->fillScreen(RGB565_BLACK);
    bgSprite->setFont(&STATION_FONT_SMALL);

    const int itemsPerPage = getStationListItemsPerPage();
    const int totalItems = getStationListTotalItems();
    const int totalPages = std::max(1, (totalItems + itemsPerPage - 1) / itemsPerPage);
    const int firstIndex = stationListPage * itemsPerPage;
    const int lineHeight = 28;
    const int listX = 8;
    const int listW = vuWidth - 16;
    const int startY = 40;

    bgSprite->setTextColor(RGB565_CYAN);
    bgSprite->setCursor(listX, 20);
    String title = stationListMode == StationListMode::Playlists
                       ? "Playlists "
                       : getPlaylistDisplayName(currentPlaylistFile) + " ";
    printDisplayText(bgSprite, title + String(stationListPage + 1) + "/" + String(totalPages));

    for (int row = 0; row < itemsPerPage; ++row) {
        const int idx = firstIndex + row;
        if (idx >= totalItems) break;

        const int rowTop = startY + row * lineHeight - 18;
        const int textY = startY + row * lineHeight;
        const bool selected = idx == stationListSelectedIndex;
        bool playing = false;

        if (selected) {
            bgSprite->fillRect(listX - 2, rowTop, listW, lineHeight - 2, RGB565_DARKGREEN);
            bgSprite->setTextColor(RGB565_WHITE);
        } else {
            bgSprite->setTextColor(RGB565_LIGHTGRAY);
        }

        String line = "";
        if (stationListMode == StationListMode::Playlists) {
            playing = stationListPlaylists[idx] == currentPlayingPlaylistFile;
            line = getPlaylistDisplayName(stationListPlaylists[idx]);
        } else if (idx > 0) {
            const int streamIdx = idx - 1;
            playing = streamIdx == currentStreamIndex && currentPlaylistFile == currentPlayingPlaylistFile;
            line = String(streamIdx + 1) + ". " + getStreamName(streamIdx);
        }
        if (!selected && playing) {
            bgSprite->setTextColor(RGB565_YELLOW);
        }
        if (line.length() > 34) line = line.substring(0, 31) + "...";
        bgSprite->setCursor(listX, textY);
        printDisplayText(bgSprite, line);
    }

    // bgSprite->setFont(&STATION_FONT_SMALL);
    // bgSprite->setTextColor(RGB565_ORANGE);
    // bgSprite->setCursor(listX, vuHeight - 8);
    // printDisplayText(bgSprite, "Next/Prev: page  Enc2: select  Click: play");

    gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)bgSprite->getFramebuffer(), vuWidth, vuHeight);
    xSemaphoreGive(xMutex);
}

void moveStationListSelection(int delta) {
    const int totalItems = getStationListTotalItems();
    if (!showStationList || totalItems <= 0 || delta == 0) return;
    stationListSelectedIndex += delta;
    if (stationListSelectedIndex < 0) {
        stationListSelectedIndex = totalItems - 1;
    } else if (stationListSelectedIndex >= totalItems) {
        stationListSelectedIndex = 0;
    }
    stationListPage = stationListSelectedIndex / getStationListItemsPerPage();
    if (!kHasNavEncoder && encoder) {
        encoder->setEncoderValue(0);
        lastEncoderValue = 0;
    }
    drawStationList();
}

void moveStationListPage(int delta) {
    const int totalItems = getStationListTotalItems();
    if (!showStationList || totalItems <= 0 || delta == 0) return;

    const int itemsPerPage = getStationListItemsPerPage();
    const int maxPage = std::max(0, (totalItems - 1) / itemsPerPage);
    stationListPage = constrain(stationListPage + delta, 0, maxPage);
    stationListSelectedIndex = constrain(stationListPage * itemsPerPage, 0, totalItems - 1);
    if (!kHasNavEncoder && encoder) {
        encoder->setEncoderValue(0);
        lastEncoderValue = 0;
    }
    drawStationList();
}

void selectStationListItem() {
    if (!showStationList) return;

    if (stationListMode == StationListMode::Playlists) {
        if (stationListSelectedIndex < 0 ||
            stationListSelectedIndex >= static_cast<int>(stationListPlaylists.size())) {
            return;
        }
        loadPlaylistMetadata(stationListPlaylists[stationListSelectedIndex]);
        openStationList();
        return;
    }

    if (stationListSelectedIndex == 0) {
        openPlaylistList();
        return;
    }

    const int target = stationListSelectedIndex - 1;
    closeStationList(false);
    selectStream(target);
    loadBackground();
    redrawScreen();
}

// === Колбэк для вывода ICY / ID3 метаданных ===
void myAudioInfo(Audio::msg_t m) {
  switch (m.e) {
    case Audio::evt_name:
      //Serial.printf("[ICY] Station: %s\n", m.msg);
      icy_station = String(m.msg);
      metadataPending = false;
      audioInfoRefreshPending = true;
      break;
    case Audio::evt_streamtitle:
      //Serial.printf("[ICY] Now playing: %s\n", m.msg);
      icy_streamtitle = String(m.msg);
      metadataPending = false;
      audioInfoRefreshPending = true;
      break;
    case Audio::evt_icyurl:
      //Serial.printf("[ICY] URL: %s\n", m.msg);
      icy_url = String(m.msg);
      break;
    case Audio::evt_bitrate:
      //Serial.printf("[ICY] Bitrate: %s kbps\n", m.msg);
      icy_bitrate = String(m.msg);
      metadataPending = false;
      audioInfoRefreshPending = true;
      break;
    case Audio::evt_id3data:
      //Serial.printf("[ID3] Tag: %s\n", m.msg);
      id3_data = String(m.msg);
      break;
    // ДОБАВЬТЕ ЭТИ КЕЙСЫ:
    case Audio::evt_info:
    {
      const char* codec = audio.getCodecname();
      if (codec) audio_codec = String(codec);
      audio_frequency = String(audio.getSampleRate()) + " Hz";
      audio_bitness = String(audio.getBitsPerSample()) + " bit";
      audioInfoRefreshPending = true;
      //Serial.printf("Audio Info: Codec=%s, Freq=%s, Bits=%s\n", 
      audio_codec.c_str(), audio_frequency.c_str(), audio_bitness.c_str();
      break;
    }

  }
}

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

// === ISR для кнопки плейлиста (пин 45) ===
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

void IRAM_ATTR encoderBtnISR() {
    unsigned long now = millis();
    if (now - lastEncoderRotationAt < ENCODER_BUTTON_ROTATION_GUARD_MS) {
        return;
    }
    
    // Антидребезг
    if (now - lastEncoderBtnPress < ENCODER_BTN_DEBOUNCE) {
        return;
    }
    
    lastEncoderBtnPress = now;
    int btnState = digitalRead(ENCODER_BTN_PIN);
    
    if (btnState == LOW) {
        // Кнопка нажата - запоминаем время начала
        encoderBtnPressStart = now;
        encoderBtnPressed = true;
    } else {
        // Кнопка отпущена
        if (encoderBtnPressed && encoderBtnPressStart > 0) {
            encoderBtnPressed = false;
            
            // Рассчитываем длительность нажатия
            unsigned long pressDuration = now - encoderBtnPressStart;
            encoderBtnPressStart = 0; // Сбрасываем

            if (pressDuration < ENCODER_MIN_VALID_PRESS_MS) {
                return;
            }
            
            if (pressDuration < LONG_PRESS_TIME) {
                // Короткое нажатие
                if (kHasNavEncoder) {
                    pendingToggle = true;
                    return;
                }
                pendingEncoderClick = true;
                
                // Проверяем, был ли это двойной клик
                if (now - lastClickTime < CLICK_TIMEOUT) {
                    pendingEncoderClicks = min(2, pendingEncoderClicks + 1);
                } else {
                    pendingEncoderClicks = 1; // Начинаем новый счет кликов
                }
                lastClickTime = now;
            }
            // Длинное нажатие больше не обрабатываем здесь, оно уже обработано в основном цикле
        }
    }
}

// === ISR для кнопки таймера сна ===
void IRAM_ATTR navEncoderBtnISR() {
    unsigned long now = millis();
    if (now - lastNavEncoderRotationAt < ENCODER_BUTTON_ROTATION_GUARD_MS) {
        return;
    }

    if (now - lastNavEncoderBtnPress < ENCODER_BTN_DEBOUNCE) {
        return;
    }

    lastNavEncoderBtnPress = now;
    int btnState = digitalRead(NAV_ENCODER_BTN_PIN);

    if (btnState == LOW) {
        navEncoderBtnPressStart = now;
        navEncoderBtnPressed = true;
    } else if (navEncoderBtnPressed && navEncoderBtnPressStart > 0) {
        unsigned long pressDuration = now - navEncoderBtnPressStart;
        navEncoderBtnPressed = false;
        navEncoderBtnPressStart = 0;
        if (pressDuration >= ENCODER_MIN_VALID_PRESS_MS && pressDuration < LONG_PRESS_TIME) {
            pendingNavEncoderClick = true;
        }
    }
}

void IRAM_ATTR sleepTimerISR() {
  unsigned long now = millis();
  if (now - lastSleepTimerISR > DEBOUNCE_MS) {
    lastSleepTimerISR = now;
    flagSleepTimer = true;
  }
}

// === Settings Save/Load ===
void savePlaybackState() {
  streamStateSavePending = false;
  prefs.begin("radio", false);
  prefs.putString("playlist", currentPlaylistFile);
  prefs.putString("playingPlaylist", currentPlayingPlaylistFile);
  prefs.putInt("stream", currentStreamIndex);
  prefs.putString("streamURL", currentStreamURL);
  prefs.putString("streamName", currentStreamName);
  prefs.end();
}

void saveRuntimeSettings() {
  prefs.begin("radio", false);
  prefs.putInt("volume", currentVolume);
  prefs.putBool("ledEnabled", ledMusicEnabled);
  prefs.putBool("vuMeterEnabled", vuMeterEnabled);
  prefs.putBool(PREF_KEY_PRIMARY_MARQUEE, primaryMarqueeEnabled);
  prefs.putBool(PREF_KEY_SECONDARY_MARQUEE, secondaryMarqueeEnabled);
  prefs.putBool(PREF_KEY_SCREENSAVER_ENABLED, screensaverEnabled);
  prefs.putInt(PREF_KEY_SCREENSAVER_IDLE, screensaverIdleSeconds);
  prefs.putInt("ledCount", ledStripCount);
  prefs.putInt("ledEffect", ledEffectIndex);
  prefs.putString("ledOrder", ledColorOrder);
  prefs.putInt("eqBalance", eqBalance);
  prefs.putInt("eqTreble", eqTreble);
  prefs.putInt("eqMid", eqMid);
  prefs.putInt("eqBass", eqBass);
  sleepTimer.saveState(prefs);
  prefs.end();
}

void saveDisplaySettings() {
  displaySettingsSavePending = false;
  prefs.begin("radio", false);
  prefs.putBool(PREF_KEY_SHOW_STATION_NUM, showStationNumber);
  prefs.putString(PREF_KEY_STATION_NUM_FMT, stationNumberFormat);
  prefs.putInt(PREF_KEY_STATION_FONT_SIZE, stationTextFontSize);
  prefs.putInt("rotation", tftRotation);
  prefs.putInt("stationW", stationScrollWidth);
  prefs.putInt("title1W", title1ScrollWidth);
  prefs.putInt("title2W", title2ScrollWidth);
  prefs.putInt("marqueePauseMs", marqueePauseMs);
  prefs.putInt("vuBarsHeight", vuBarsHeight);
  prefs.putInt("vuBarsSegments", vuBarsSegments);
  prefs.putInt("vuBarsOffsetX", vuBarsOffsetX);
  prefs.putInt("vuBarsOffsetY", vuBarsOffsetY);
  prefs.putUShort("vuBarsColorB", vuBarsColorBottom);
  prefs.putUShort("vuBarsColorT", vuBarsColorTop);
  prefs.putInt("vuCardioX", vuCardioX);
  prefs.putInt("vuCardioY", vuCardioY);
  prefs.putInt("vuCardioW", vuCardioW);
  prefs.putInt("vuCardioH", vuCardioH);
  prefs.putInt("fftX", fftWindowX);
  prefs.putInt("fftY", fftWindowY);
  prefs.putInt("fftW", fftWindowW);
  prefs.putInt("fftH", fftWindowH);
  prefs.putInt("fftBands", fftBands);
  prefs.putInt("fftSegments", fftSegments);
  prefs.putInt("fftMinBin", fftMinBin);
  prefs.putInt("fftMaxBin", fftMaxBin);
  prefs.putInt("fftAggr", fftAggregation);
  prefs.putInt("fftCurve", fftCurve);
  prefs.putInt("fftProfile", fftProfile);
  prefs.putFloat("fftGain", fftGain);
  prefs.putFloat("fftPreGain", fftPreGain);
  prefs.putFloat("fftGate", fftNoiseGate);
  prefs.putFloat("fftFloor", fftNoiseFloor);
  prefs.putFloat("fftSens", fftSensitivity);
  prefs.putFloat("fftGamma", fftGamma);
  prefs.putFloat("fftLog", fftLogStrength);
  prefs.putFloat("fftAttack", fftAttack);
  prefs.putFloat("fftRelease", fftRelease);
  prefs.putFloat("fftLow", fftLowBoost);
  prefs.putFloat("fftMid", fftMidBoost);
  prefs.putFloat("fftHigh", fftHighBoost);
  prefs.putInt("fftGap", fftBarGap);
  prefs.putInt("fftSide", fftSideMargin);
  prefs.putInt("fftTop", fftTopMargin);
  prefs.putInt("fftBottom", fftBottomMargin);
  prefs.putInt("fftMinLit", fftMinLitSegments);
  prefs.putBool("fftPeakHold", fftPeakHold);
  prefs.putInt("fftPeakFall", fftPeakFall);
  prefs.putBool("fftMirror", fftMirror);
  prefs.putBool("fftInvert", fftInvert);
  prefs.putBool("displayInvert", displayInverted);
  prefs.putUShort("fftColorB", fftColorBottom);
  prefs.putUShort("fftColorT", fftColorTop);
  prefs.putUShort("fftColorP", fftColorPeak);
  prefs.putUShort("fftColorO", fftColorOff);
  size_t screenDefCount = 0;
  const ScreenDefinition* screenDefs = getAllScreenDefinitions(screenDefCount);
  for (size_t i = 0; i < screenDefCount; ++i) {
    String key = makeScreenEnabledPrefKey(screenDefs[i].id);
    prefs.putBool(key.c_str(), isScreenEnabled(screenDefs[i].id));
  }
  prefs.end();
  saveCurrentScreenSettings();
  saveLastActiveScreen();
  //Serial.println("✅ Settings saved");
  //Serial.printf("💾 Saved station: idx=%d, name=%s\n", currentStreamIndex, currentStreamName.c_str());
  //Serial.printf("💾 Saved URL: %s\n", currentStreamURL.c_str());
}

void saveSettings() {
  savePlaybackState();
  saveRuntimeSettings();
  saveDisplaySettings();
}

void scheduleDisplaySettingsSave() {
  displaySettingsSavePending = true;
}

void scheduleStreamStateSave() {
  streamStateSavePending = true;
  streamStateSaveAt = millis() + STREAM_STATE_SAVE_DELAY_MS;
}

void saveVolumeSetting() {
  volumeSavePending = false;
  prefs.begin("radio", false);
  prefs.putInt("volume", currentVolume);
  prefs.end();
}

void scheduleVolumeSave() {
  volumeSavePending = true;
  volumeSaveAt = millis() + VOLUME_SAVE_DELAY_MS;
}

void servicePendingVolumeSave() {
  if (!volumeSavePending) return;
  const long remaining = static_cast<long>(volumeSaveAt - millis());
  if (remaining > 0) return;
  saveVolumeSetting();
}

void servicePendingStreamStateSave() {
  if (!streamStateSavePending) return;
  const long remaining = static_cast<long>(streamStateSaveAt - millis());
  if (remaining > 0) return;
  if (metadataPending) return;
  savePlaybackState();
}

void flushPendingDisplaySettingsSave() {
  if (!displaySettingsSavePending) return;
  saveDisplaySettings();
  setTftBrightness(getTftBrightness(), true);
}

String makeScreenEnabledPrefKey(ScreenId id) {
  String key = "se_" + String(getScreenKey(id));
  if (key.length() > 15) key = key.substring(0, 15);
  return key;
}

void loadSettings() {
  prefs.begin("radio", true);
  currentPlaylistFile = prefs.getString("playlist", "/playlist.csv");
  currentPlayingPlaylistFile = prefs.getString("playingPlaylist", currentPlaylistFile);
  currentStreamIndex = prefs.getInt("stream", 0);
  currentVolume = constrain(prefs.getInt("volume", AUDIO_DEFAULT_VOLUME),
                            kVolumePercentMin,
                            kVolumePercentMax);
  ledMusicEnabled = prefs.getBool("ledEnabled", true);
  vuMeterEnabled = prefs.getBool("vuMeterEnabled", true);
  primaryMarqueeEnabled = prefs.isKey(PREF_KEY_PRIMARY_MARQUEE)
                              ? prefs.getBool(PREF_KEY_PRIMARY_MARQUEE, true)
                              : true;
  secondaryMarqueeEnabled = prefs.isKey(PREF_KEY_SECONDARY_MARQUEE)
                                ? prefs.getBool(PREF_KEY_SECONDARY_MARQUEE, false)
                                : false;
  screensaverEnabled = prefs.getBool(PREF_KEY_SCREENSAVER_ENABLED, false);
  screensaverIdleSeconds = constrain(prefs.getInt(PREF_KEY_SCREENSAVER_IDLE, 60), 5, 3600);
  ledStripCount = prefs.getInt("ledCount", WS2812B_LED_COUNT);
  ledEffectIndex = prefs.getInt("ledEffect", 0);
  ledColorOrder = prefs.getString("ledOrder", LED_COLOR_ORDER);
  eqBalance = constrain(prefs.getInt("eqBalance", 0), -16, 16);
  eqTreble = constrain(prefs.getInt("eqTreble", 0), -12, 12);
  eqMid = constrain(prefs.getInt("eqMid", 0), -12, 12);
  eqBass = constrain(prefs.getInt("eqBass", 0), -12, 12);
  // Station number display
  showStationNumber = prefs.isKey(PREF_KEY_SHOW_STATION_NUM)
                          ? prefs.getBool(PREF_KEY_SHOW_STATION_NUM, true)
                          : true;
  stationNumberFormat = prefs.isKey(PREF_KEY_STATION_NUM_FMT)
                            ? prefs.getString(PREF_KEY_STATION_NUM_FMT, "{cur}")
                            : "{cur}";
  stationTextFontSize = constrain(prefs.getInt(PREF_KEY_STATION_FONT_SIZE, 1), 0, 2);
  tftRotation = prefs.getInt("rotation", DISPLAY_PROFILE_ROTATION);
  stationScrollWidth = prefs.getInt("stationW", 0);
  title1ScrollWidth = prefs.getInt("title1W", 0);
  title2ScrollWidth = prefs.getInt("title2W", 0);
  marqueePauseMs = constrain(prefs.getInt("marqueePauseMs", 1500), 0, 10000);
  vuBarsHeight = constrain(prefs.getInt("vuBarsHeight", 34), 6, 120);
  vuBarsSegments = constrain(prefs.getInt("vuBarsSegments", 15), 4, 40);
  vuBarsOffsetX = constrain(prefs.getInt("vuBarsOffsetX", 0), -200, 200);
  vuBarsOffsetY = constrain(prefs.getInt("vuBarsOffsetY", 0), -200, 200);
  vuBarsColorBottom = prefs.getUShort("vuBarsColorB", RGB565_LIME);
  vuBarsColorTop = prefs.getUShort("vuBarsColorT", RGB565_RED);
  vuCardioX = prefs.getInt("vuCardioX", 8);
  vuCardioY = prefs.getInt("vuCardioY", 92);
  vuCardioW = constrain(prefs.getInt("vuCardioW", 304), 80, 480);
  vuCardioH = constrain(prefs.getInt("vuCardioH", 104), 40, 320);

  fftWindowX = prefs.getInt("fftX", 10);
  fftWindowY = prefs.getInt("fftY", 240);
  fftWindowW = constrain(prefs.getInt("fftW", 320), 40, 480);
  fftWindowH = constrain(prefs.getInt("fftH", 100), 40, 320);
  fftBands = constrain(prefs.getInt("fftBands", 32), 1, FFT_BANDS);
  fftSegments = constrain(prefs.getInt("fftSegments", 50), 1, 80);
  fftMinBin = constrain(prefs.getInt("fftMinBin", 0), 0, FFT_BANDS - 1);
  fftMaxBin = constrain(prefs.getInt("fftMaxBin", FFT_BANDS - 1), fftMinBin, FFT_BANDS - 1);
  fftAggregation = constrain(prefs.getInt("fftAggr", 0), 0, 2);
  fftCurve = constrain(prefs.getInt("fftCurve", 1), 0, 2);
  fftProfile = constrain(prefs.getInt("fftProfile", 2), 0, 2);
  fftGain = constrain(prefs.getFloat("fftGain", 1.5f), 0.0f, 8.0f);
  fftPreGain = constrain(prefs.getFloat("fftPreGain", 1.0f), 0.0f, 8.0f);
  fftNoiseGate = constrain(prefs.getFloat("fftGate", 0.0f), 0.0f, 120.0f);
  fftNoiseFloor = constrain(prefs.getFloat("fftFloor", 25.0f), 0.0f, 160.0f);
  fftSensitivity = constrain(prefs.getFloat("fftSens", 1.0f), 0.0f, 4.0f);
  fftGamma = constrain(prefs.getFloat("fftGamma", 0.75f), 0.1f, 3.0f);
  fftLogStrength = constrain(prefs.getFloat("fftLog", 6.0f), 0.1f, 12.0f);
  fftAttack = constrain(prefs.getFloat("fftAttack", 0.75f), 0.0f, 1.0f);
  fftRelease = constrain(prefs.getFloat("fftRelease", 0.35f), 0.0f, 1.0f);
  fftLowBoost = constrain(prefs.getFloat("fftLow", 1.6f), 0.0f, 4.0f);
  fftMidBoost = constrain(prefs.getFloat("fftMid", 1.7f), 0.0f, 4.0f);
  fftHighBoost = constrain(prefs.getFloat("fftHigh", 1.3f), 0.0f, 4.0f);
  fftBarGap = constrain(prefs.getInt("fftGap", 5), 0, 12);
  fftSideMargin = constrain(prefs.getInt("fftSide", 0), 0, 80);
  fftTopMargin = constrain(prefs.getInt("fftTop", 0), 0, 100);
  fftBottomMargin = constrain(prefs.getInt("fftBottom", 0), 0, 100);
  fftMinLitSegments = constrain(prefs.getInt("fftMinLit", 0), 0, 10);
  fftPeakHold = prefs.getBool("fftPeakHold", true);
  fftPeakFall = constrain(prefs.getInt("fftPeakFall", 5), 0, 20);
  fftMirror = prefs.getBool("fftMirror", false);
  fftInvert = prefs.getBool("fftInvert", false);
  displayInverted = prefs.getBool("displayInvert", false);
  fftColorBottom = prefs.getUShort("fftColorB", RGB565(255, 255, 255));
  fftColorTop = prefs.getUShort("fftColorT", RGB565_RED);
  fftColorPeak = prefs.getUShort("fftColorP", RGB565_WHITE);
  fftColorOff = prefs.getUShort("fftColorO", RGB565(0, 28, 0));
  size_t screenDefCount = 0;
  const ScreenDefinition* screenDefs = getAllScreenDefinitions(screenDefCount);
  for (size_t i = 0; i < screenDefCount; ++i) {
    String key = makeScreenEnabledPrefKey(screenDefs[i].id);
    setScreenEnabled(screenDefs[i].id, prefs.getBool(key.c_str(), screenDefs[i].enabledByDefault));
  }
  prefs.end();
  if (gfx) {
    gfx->setRotation(tftRotation);
    gfx->invertDisplay(displayInverted);
  }
  loadLastActiveScreen();
  ensureCurrentScreenEnabled();
  loadCurrentScreenSettings();
  //Serial.println("✅ Settings loaded");
}

void applyAudioEq() {
  audio.setBalance(static_cast<float>(eqBalance));
  audio.setTone(static_cast<float>(eqBass), static_cast<float>(eqMid), static_cast<float>(eqTreble));
}

// === WiFi ===
struct WiFiCredential {
  String ssid;
  String password;
};

bool connectToWiFi() {
  WiFi.setSleep(false);

  WiFiCredential credentials[3];
  Preferences wifiPrefs;
  wifiPrefs.begin("radio", true);
  for (int i = 0; i < 3; ++i) {
    String ssidKey = String("wifi_ssid") + String(i + 1);
    String passKey = String("wifi_pass") + String(i + 1);
    credentials[i].ssid = wifiPrefs.getString(ssidKey.c_str(), "");
    credentials[i].password = wifiPrefs.getString(passKey.c_str(), "");
  }

  if (credentials[0].ssid.length() == 0) {
    credentials[0].ssid = wifiPrefs.getString("wifi_ssid", "");
    credentials[0].password = wifiPrefs.getString("wifi_pass", "");
  }
  wifiPrefs.end();

#ifdef DEFAULT_WIFI_SSID
  bool hasSavedNetwork = false;
  for (const auto& credential : credentials) {
    if (credential.ssid.length() > 0) {
      hasSavedNetwork = true;
      break;
    }
  }
  if (!hasSavedNetwork) {
    credentials[0].ssid = DEFAULT_WIFI_SSID;
    credentials[0].password = DEFAULT_WIFI_PASSWORD;
  }
#endif

  for (int i = 0; i < 3; ++i) {
    if (credentials[i].ssid.length() == 0) continue;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    delay(150);
    WiFi.begin(credentials[i].ssid.c_str(), credentials[i].password.c_str());
    Serial.print("Connecting WiFi: ");
    Serial.println(credentials[i].ssid);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true;

      Preferences prefsWrite;
      prefsWrite.begin("radio", false);
      prefsWrite.putString("wifi_ssid", credentials[i].ssid);
      prefsWrite.putString("wifi_pass", credentials[i].password);
      if (i == 0) {
        prefsWrite.putString("wifi_ssid1", credentials[i].ssid);
        prefsWrite.putString("wifi_pass1", credentials[i].password);
      }
      prefsWrite.end();
      return true;
    }
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("S3-Go!-light-Setup");
  Serial.print("Setup AP IP: ");
  //Serial.println(WiFi.softAPIP());
  wifiConnected = false;
  return false;
}

void showCenteredSplash(const String& text, uint16_t color, unsigned long durationMs) {
  if (!gfx) return;

  if (!drawBootImageBackground()) {
    gfx->fillScreen(RGB565_BLACK);
  }
  gfx->setFont(&STATION_FONT_NORMAL);
  gfx->setTextColor(color);

  int16_t x1, y1;
  uint16_t w, h;
  getDisplayTextBounds(gfx, text, 0, 0, &x1, &y1, &w, &h);

  int drawX = ((int)gfx->width() - (int)w) / 2 - x1;
  int drawY = ((int)gfx->height() - (int)h) / 2 - y1;
  gfx->setCursor(drawX, drawY);
  printDisplayText(gfx, text);

  if (durationMs > 0) {
    delay(durationMs);
  }
}

void showStartupIpSplash() {
  String ipText = "No IP";
  unsigned long splashDuration = 1000;
  
  if (wifiConnected) {
    ipText = WiFi.localIP().toString();
  } else if (WiFi.getMode() == WIFI_AP) {
    ipText = "AP: " + WiFi.softAPIP().toString();
    splashDuration = 0;
  }

  if (!gfx) return;

  if (!drawBootImageBackground()) {
    gfx->fillScreen(RGB565_BLACK);
  }
  
  gfx->setFont(&STATION_FONT_NORMAL);
  gfx->setTextColor(colorIP);
  
  // Получаем размеры текста для центрирования
  int16_t x1, y1;
  uint16_t w, h;
  getDisplayTextBounds(gfx, ipText, 0, 0, &x1, &y1, &w, &h);
  
  // X: центр экрана минус половина ширины текста
  int ipX = (gfx->width() - w) / 2;
  // Y: 80% от высоты экрана (без привязки к высоте текста)
  int ipY = (gfx->height() * 90) / 100;
  
  gfx->setCursor(ipX, ipY);
  printDisplayText(gfx, ipText);

  if (splashDuration > 0) {
    delay(splashDuration);
  }
}

void showWiFiSetupWaitingScreen() {
  if (!gfx) return;

  if (!drawBootImageBackground()) {
    gfx->fillScreen(RGB565_BLACK);
  }
  gfx->setFont(&STATION_FONT_NORMAL);
  gfx->setTextColor(colorIP);
  gfx->setCursor(12, 70);
  printDisplayText(gfx, "WiFi setup mode");

  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(12, 110);
  printDisplayText(gfx, "Connect to AP:");
  gfx->setCursor(12, 140);
  printDisplayText(gfx, "S3-Go!-light-Setup");

  gfx->setTextColor(RGB565_GREEN);
  gfx->setCursor(12, 180);
  printDisplayText(gfx, "Open:");
  gfx->setCursor(12, 210);
  printDisplayText(gfx, WiFi.softAPIP().toString());
}

void applyCurrentVolume() {
    // Логарифмическая кривая из Audio.cpp
    float t = currentVolume / 100.0f;
    float dB = -112.0f * t * t * t + 172.0f * t * t - 60.0f;
    float vol = powf(10.0f, dB / 20.0f);
    int volumeAudio = (int)(vol * 255.0f);
    
    int effectiveVolume = constrain(volumeAudio + currentStationOvol, 0, 255);
    audio.setVolume(effectiveVolume);
}

void handleVolumeStep(int delta) {
    noteUserActivity();
    int newVolume = constrain(currentVolume + delta, 0, 100);
    if (newVolume == currentVolume) return;

    currentVolume = newVolume;
    applyCurrentVolume();
    scheduleVolumeSave();
    
    if (encoder && !showStationList) {
        encoder->setEncoderValue(currentVolume);  // Синхронизация
        lastEncoderValue = currentVolume;
    }
}

bool getStreamByIndexWithOvol(int idx, String &outName, String &outURL, int &outOvol) {
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
  outOvol = 0;

  String ovol = line.substring(tab2 + 1);
  ovol.trim();
  if (ovol.length() > 0) {
    outOvol = ovol.toInt();
  }
  
  //Serial.printf("✅ Loaded stream #%d: %s -> %s\n", idx, outName.c_str(), outURL.c_str());
  return true;
}

bool getStreamByIndex(int idx, String &outName, String &outURL) {
  int unusedOvol = 0;
  return getStreamByIndexWithOvol(idx, outName, outURL, unusedOvol);
}

void loadPlaylistMetadata(String filename) {
  if (!filename.startsWith("/")) filename = "/" + filename;

  if (!LittleFS.exists(filename)) {
    //Serial.printf("⚠️ Playlist file %s not found - creating empty playlist\n", filename.c_str());
    currentPlaylistFile = filename;
    streamCount = 0;
    playlistIndex.clear();  // Очищаем вектор
    return;
  }

  currentPlaylistFile = filename;
  playlistIndex.clear();  // Очищаем старые данные
  
  File f = LittleFS.open(filename, "r");
  if (!f) {
    //Serial.printf("⚠️ Failed to open playlist file %s\n", filename.c_str());
    streamCount = 0;
    return;
  }
  
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    if (tab1 == -1 || tab2 == -1) continue;
    
    PlaylistEntry entry;
    entry.name = line.substring(0, tab1);
    playlistIndex.push_back(entry);  // Добавляем в вектор
  }
  
  f.close();
  streamCount = playlistIndex.size();  // Обновляем счетчик
  
  //Serial.printf("✅ Playlist loaded: %d streams (metadata only, URLs on demand)\n", streamCount);
}

// === Обновленная функция selectStream ===
bool shouldShowMetadataScreen() {
  return getCurrentScreen() == ScreenId::StationInfo;
}

void selectStream(int idx) {
  // Immediately show the selected station name from the playlist,
  // but perform the actual connection after debounce (to avoid
  // reconnecting on rapid next/prev presses).
  noteUserActivity();
  if (idx < 0 || idx >= streamCount) return;
  // update displayed name right away
  currentStreamName = getStreamName(idx);
  redrawScreen();
  // schedule actual connection
  pendingSelectedStreamIndex = idx;
  pendingSelectRequestTime = millis();
}

// Immediate selection: performs the actual connection to the stream URL
void selectStreamNow(int idx) {
  noteUserActivity();
  if (idx < 0 || idx >= streamCount) return;

  currentStreamIndex = idx;
  currentPlayingPlaylistFile = currentPlaylistFile;

  if (getStreamByIndexWithOvol(idx, currentStreamName, currentStreamURL, currentStationOvol)) {
    icy_station = "";
    icy_streamtitle = "";
    icy_bitrate = "";
    audio_codec = "";
    audio_frequency = "";
    audio_bitness = "";

    metadataPending = true;
    metadataRequestTime = millis();

    applyCurrentVolume();
    audio.connecttohost(currentStreamURL.c_str());
    radioStarted = true;
    isPaused = false;

    scheduleStreamStateSave();
  }
}
String getStreamName(int idx) {
  if(idx >= 0 && idx < streamCount) {
    return playlistIndex[idx].name;
  }
  return "Unknown";
}

void nextStream() {
  noteUserActivity();
  if (showStationList) {
    moveStationListPage(1);
    return;
  }
  if(streamCount > 0){
    currentStreamIndex++;
    if(currentStreamIndex >= streamCount) currentStreamIndex = 0;
    selectStream(currentStreamIndex);
  }
}

void prevStream() {
  noteUserActivity();
  if (showStationList) {
    moveStationListPage(-1);
    return;
  }
  if(streamCount > 0){
    currentStreamIndex--;
    if(currentStreamIndex < 0) currentStreamIndex = streamCount - 1;
    selectStream(currentStreamIndex);
  }
}

bool activatePlaylistByIndex(int idx) {
  std::vector<String> playlists;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    if (name.endsWith(".csv")) playlists.push_back(name);
    file = root.openNextFile();
  }

  if (playlists.empty()) return false;

  const int playlistCount = static_cast<int>(playlists.size());
  if (idx < 0) {
    idx = (idx % playlistCount + playlistCount) % playlistCount;
  } else if (idx >= playlistCount) {
    idx %= playlistCount;
  }

  loadPlaylistMetadata(playlists[idx]);
  if (streamCount > 0) {
    selectStream(0);
  } else {
    savePlaybackState();
  }

  closeStationList(false);
  redrawScreen();
  return true;
}

bool selectNextPlaylist() {
  std::vector<String> playlists;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    if (name.endsWith(".csv")) playlists.push_back(name);
    file = root.openNextFile();
  }
  if (playlists.empty()) return false;

  int selectedPlaylistIndex = 0;
  for (int i = 0; i < static_cast<int>(playlists.size()); ++i) {
    if (playlists[i] == currentPlaylistFile) {
      selectedPlaylistIndex = i;
      break;
    }
  }
  return activatePlaylistByIndex(selectedPlaylistIndex + 1);
}

void cycleToNextScreen() {
    noteUserActivity();
    // Временно убираем мьютекс
    // if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    //     return;
    // }
    
    saveCurrentScreenSettings();
    bool moved = moveToNextScreen();
    
    if (moved) {
        loadCurrentScreenSettings();
        loadBackground();  // loadBackground теперь должна сама брать мьютекс
        saveLastActiveScreen();
        redrawScreenLocked();
    }
    
    // xSemaphoreGive(xMutex);
}

void checkStreamChange() {
  if (streamChangeRequested && streamCount > 0) {
    nextStream();
    streamChangeRequested = false;
  }
}


void handleVolumeEncoderRotation() {
    if (!encoder) return;

    const long diff = encoder->encoderChanged();
    if (diff == 0) return;

    if (!kHasNavEncoder && showStationList) {
        lastEncoderRotationAt = millis();
        moveStationListSelection(diff > 0 ? 1 : -1);
        encoder->setEncoderValue(0);
        lastEncoderValue = 0;
        return;
    }

    lastEncoderRotationAt = millis();
    handleVolumeStep(static_cast<int>(diff));
}

void handleNavEncoderRotation() {
    if (!navEncoder) return;

    long diff = navEncoder->encoderChanged();
    if (diff == 0) return;

    lastNavEncoderRotationAt = millis();
    const int step = diff > 0 ? 1 : -1;

    if (showStationList) {
        moveStationListSelection(step);
        navEncoder->setEncoderValue(0);
        lastNavEncoderValue = 0;
        return;
    }

    if (step > 0) {
        nextStream();
    } else {
        prevStream();
    }

    navEncoder->setEncoderValue(0);
    lastNavEncoderValue = 0;
}

void handleNavEncoderButton() {
    if (!pendingNavEncoderClick) return;

    pendingNavEncoderClick = false;
    noteUserActivity();

    if (showStationList) {
        selectStationListItem();
        return;
    }

    cycleToNextScreen();
}

void handleLedControlButtons() {
#ifdef WS2812B_PIN
    unsigned long now = millis();

    if (LED_TOGGLE_BUTTON_PIN != 255) {
        bool state = digitalRead(LED_TOGGLE_BUTTON_PIN);
        if (lastLedToggleButtonState == HIGH && state == LOW && now - lastLedToggleButtonAt > DEBOUNCE_MS) {
            noteUserActivity();
            lastLedToggleButtonAt = now;
            ledMusicEnabled = !ledMusicEnabled;
            if (!ledMusicEnabled) {
                ledMusic.off();
            }
            saveRuntimeSettings();
            Serial.println(String("LED ") + (ledMusicEnabled ? "enabled" : "disabled"));
        }
        lastLedToggleButtonState = state;
    }

    if (LED_EFFECT_BUTTON_PIN != 255) {
        bool state = digitalRead(LED_EFFECT_BUTTON_PIN);
        if (lastLedEffectButtonState == HIGH && state == LOW && now - lastLedEffectButtonAt > DEBOUNCE_MS) {
            noteUserActivity();
            lastLedEffectButtonAt = now;
            ledMusic.nextEffect();
            ledEffectIndex = ledMusic.getEffectIndex();
            saveRuntimeSettings();
            Serial.println("LED effect: " + String(ledMusic.getEffectName()));
        }
        lastLedEffectButtonState = state;
    }
#endif
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  warmupGpioIsrService();

  if (BTN_PLAY_PAUSE_PIN != 255 && BTN_PLAY_PAUSE_PIN == ENCODER_BTN_PIN) {
    Serial.printf("[WARN] BTN_PLAY_PAUSE_PIN (%d) conflicts with ENCODER_BTN_PIN, dedicated play/pause button disabled.\n",
                  BTN_PLAY_PAUSE_PIN);
  }
  if (NAV_ENCODER_BTN_PIN != 255 && NAV_ENCODER_BTN_PIN == ENCODER_BTN_PIN) {
    Serial.printf("[WARN] NAV_ENCODER_BTN_PIN (%d) conflicts with ENCODER_BTN_PIN.\n",
                  NAV_ENCODER_BTN_PIN);
  }
  if (!kHasNavEncoder && NAV_ENCODER_BTN_PIN != 255) {
    Serial.printf("[WARN] NAV encoder button pin (%d) is set, but NAV encoder A/B pins are disabled. Single-encoder logic will be used.\n",
                  NAV_ENCODER_BTN_PIN);
  }
  if (psramInit()) {
    //Serial.printf("PSRAM: OK, size: %d bytes\n", ESP.getPsramSize());
  } else {
    //Serial.println("ERROR: PSRAM not initialized!");
  
  }
  xMutex = xSemaphoreCreateMutex();
  
  // === TFT Display ===
  // Явно указываем частоту SPI для дисплея NV3007 — тест: 80 MHz
  gfx->begin(80000000);
  
  // !!! ВАЖНО: Сразу заполняем черным цветом !!!
  // Keep the backlight off until the first boot frame is ready.
  
  // Устанавливаем подсветку
  initTftBacklight(true);

  // === File System ===
  if (!LittleFS.begin(false)) {
    //Serial.println("LittleFS mount failed - trying to format...");
    if (LittleFS.begin(true)) {
      //Serial.println("✅ LittleFS formatted and mounted successfully!");
    } else {
      //Serial.println("❌ LittleFS format failed!");
      tftBacklightOn();
      gfx->fillScreen(RGB565_RED);
      gfx->setFont(&STATION_FONT_NORMAL);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setCursor(20, 100);
      printDisplayText(gfx, "FS ERROR!");
      gfx->setCursor(20, 130);
      printDisplayText(gfx, "Check partition");
      while (true) delay(1000);
    }
  }
  //Serial.println("LittleFS mounted");
  //Serial.printf("Total space: %d bytes\n", LittleFS.totalBytes());
  //Serial.printf("Used space: %d bytes\n", LittleFS.usedBytes());
  //Serial.printf("Free space: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
  if (!drawBootImageBackground()) {
    gfx->fillScreen(RGB565_BLACK);
  }
  tftBacklightOn();

  // === Load Settings ===
  initScreenManager();
  irRemoteLoadConfig();
  loadSettings();
  lastUserActivityMs = millis();
  cityScreensaver = new CityScreensaver(gfx);
  if (cityScreensaver) {
    cityScreensaver->begin(false);
  }

  // === КНОПКИ (БЫЛИ ЗДЕСЬ) ===
  if (BTN_VOL_UP_PIN != 255) pinMode(BTN_VOL_UP_PIN, INPUT_PULLUP);
  if (BTN_VOL_DOWN_PIN != 255) pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);
  if (BTN_NEXT_PIN != 255) pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  if (BTN_PREV_PIN != 255) pinMode(BTN_PREV_PIN, INPUT_PULLUP);
  if (BTN_PLAY_PAUSE_PIN != 255 && BTN_PLAY_PAUSE_PIN != ENCODER_BTN_PIN) {
    pinMode(BTN_PLAY_PAUSE_PIN, INPUT_PULLUP);
  }
#ifdef WS2812B_PIN
  if (LED_TOGGLE_BUTTON_PIN != 255) pinMode(LED_TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  if (LED_EFFECT_BUTTON_PIN != 255) pinMode(LED_EFFECT_BUTTON_PIN, INPUT_PULLUP);
#endif

  if (SLEEP_TIMER_BUTTON_PIN != 255) {
    pinMode(SLEEP_TIMER_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SLEEP_TIMER_BUTTON_PIN), sleepTimerISR, FALLING);
  }

  if (BTN_VOL_UP_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_VOL_UP_PIN), volUpISR, FALLING);
  if (BTN_VOL_DOWN_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_VOL_DOWN_PIN), volDownISR, FALLING);
  if (BTN_NEXT_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_NEXT_PIN), nextStreamISR, FALLING);
  if (BTN_PREV_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_PREV_PIN), prevStreamISR, FALLING);
  if (BTN_PLAY_PAUSE_PIN != 255 && BTN_PLAY_PAUSE_PIN != ENCODER_BTN_PIN) {
    attachInterrupt(digitalPinToInterrupt(BTN_PLAY_PAUSE_PIN), playPauseISR, FALLING);
  }

  // === Rotary Encoder (ВАЖНО!) ===
  if (kHasVolumeEncoder) {
    encoder = new yoEncoder(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_STEPS, ENCODER_INTERNAL_PULLUP);
    encoder->setBoundaries(kVolumePercentMin, kVolumePercentMax, false);
    encoder->disableAcceleration();
    encoder->setup([]() {
        if (encoder) {
            encoder->readEncoder_ISR();
        }
    });
    encoder->begin();
    encoder->setEncoderValue(currentVolume);
    lastEncoderValue = currentVolume;
  }

    // Кнопка энкодера — используем OneButton
    if (ENCODER_BTN_PIN != 255) {
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    encoderBtn = new OneButton(ENCODER_BTN_PIN, true, true);
    encoderBtn->setDebounceTicks(ENCODER_BTN_DEBOUNCE);
    encoderBtn->setClickTicks(CLICK_TIMEOUT);
    encoderBtn->setPressTicks(LONG_PRESS_TIME);
    encoderBtn->attachClick([](void* p) {
      if (kHasNavEncoder) {
        pendingToggle = true;
        return;
      }
      if (showStationList) {
        selectStationListItem();
      } else {
        nextStream();
      }
    }, nullptr);
    encoderBtn->attachDoubleClick([](void* p) {
      if (!kHasNavEncoder) prevStream();
    }, nullptr);
    encoderBtn->attachLongPressStart([](void* p) {
      pendingEncoderLongPress = true;
    }, nullptr);
    }

  if (kHasNavEncoder) {
    navEncoder = new yoEncoder(NAV_ENCODER_A_PIN, NAV_ENCODER_B_PIN, ENCODER_STEPS, NAV_ENCODER_INTERNAL_PULLUP);
    navEncoder->setBoundaries(-100, 100, false);
    navEncoder->disableAcceleration();
    navEncoder->setup([]() {
        if (navEncoder) {
            navEncoder->readEncoder_ISR();
        }
    });
    navEncoder->begin();
    navEncoder->setEncoderValue(0);
    lastNavEncoderValue = 0;
  }

  if (kHasNavEncoder && kHasNavEncoderButton) {
    pinMode(NAV_ENCODER_BTN_PIN, INPUT_PULLUP);
    navEncoderBtn = new OneButton(NAV_ENCODER_BTN_PIN, true, true);
    navEncoderBtn->setDebounceTicks(ENCODER_BTN_DEBOUNCE);
    navEncoderBtn->setClickTicks(CLICK_TIMEOUT);
    navEncoderBtn->setPressTicks(LONG_PRESS_TIME);
    navEncoderBtn->attachClick([](void* p) {
        pendingNavEncoderClick = true;
    }, nullptr);
    navEncoderBtn->attachLongPressStart([](void* p) {
        pendingNavEncoderLongPress = true;
    }, nullptr);
  }

  irRemoteBegin();

  // === JPEG Decoder ===
  TJpgDec.setJpgScale(1);

  // === Sprites ===
  createSprites();
  loadBackground();

  // Показываем "S3-Go!" на указанных координатах
  if (gfx) {
    const char* text = "S3-Go!";
    gfx->setFont(&STATION_FONT_NORMAL);
    gfx->setTextColor(RGB565_WHITE);
    
    // Получаем размеры текста для центрирования
    int16_t x1, y1;
    uint16_t w, h;
    getDisplayTextBounds(gfx, text, 0, 0, &x1, &y1, &w, &h);
    
    // X: центр экрана минус половина ширины текста
    int splashX = (gfx->width() - w) / 2;
    // Y: 80% от высоты экрана
    int splashY = (gfx->height() * 90) / 100;
    
    gfx->setCursor(splashX, splashY);
    printDisplayText(gfx, text);
  }
  
  // === WiFi ===
  bool wifi_ok = connectToWiFi();
  showStartupIpSplash();
 
  // === Web Server ===
  initWebServer();

  if (!wifi_ok && WiFi.getMode() == WIFI_AP) {
    showWiFiSetupWaitingScreen();
    return;
  }
  
  // === Playlist ===
  loadPlaylistMetadata(currentPlaylistFile);
  
  // if (streamCount == 0) {
  //   //Serial.println("No streams in playlist. Add playlist.csv via web interface!");
  //   gfx->setFont(&STATION_FONT_NORMAL);
  //   gfx->setTextColor(RGB565_YELLOW);
  //   gfx->setCursor(10, 100);
  //   printDisplayText(gfx, "No playlist found");
  //   gfx->setCursor(10, 130);
  //   printDisplayText(gfx, "Upload playlist.csv");
  //   gfx->setCursor(10, 160);
  //   printDisplayText(gfx, "via web interface");
  // }
  
  // === Audio ===
  audio.setPinout(AUDIO_I2S_BCLK, AUDIO_I2S_LRCLK, AUDIO_I2S_DOUT);
  audio.setSpectrumProfile(fftProfile);
  //Serial.printf("Before getInBufferSize: %d KB\n", audio.getInBufferSize() / 1024);
  applyAudioEq();
  configureAudioVolumeRange(audio);  // Устанавливаем 255 шагов
  applyCurrentVolume();
  
    // === LED Music ===
  #ifdef WS2812B_PIN
    if (!ledMusic.begin(ledStripCount)) {
      Serial.println("⚠️ Failed to initialize LED Music");
    } else {
      ledMusic.setColorOrder(ledColorOrder.c_str());
      ledColorOrder = ledMusic.getColorOrderName();
      ledMusic.setEffectIndex(ledEffectIndex);
      ledStripCount = ledMusic.getLedCount();
      ledEffectIndex = ledMusic.getEffectIndex();
      Serial.println("✅ LED Music ready");
    }
  #endif
    
  Audio::audio_info_callback = myAudioInfo;
  
  // === Clock ===
  initClock();
  
  // === Display Task ===
  xTaskCreatePinnedToCore(displayTask, "Display Task", 4096, NULL, 2, NULL, 1);
  
  // === Auto-start last station ===
  if(streamCount > 0 && wifiConnected){
    startRadio();
  } else {
    //Serial.println("⚠️ Radio not started: no playlist or WiFi");
  }
}
  
// === Encoder long press check ===
void checkEncoderLongPress() {
  // Long-press detection handled by OneButton callbacks now.
  (void)kHasNavEncoder;
  return;
}

void checkNavEncoderLongPress() {
  // Long-press detection handled by OneButton callbacks now.
  (void)kHasNavEncoder;
  return;
}

void loop() {
    server.handleClient();
    
    // ✅ Обработка ИК-команд с высоким приоритетом
    irRemoteLoop();
    
    // ✅ Отложенные действия
    if (pendingScreenChange) {
        pendingScreenChange = false;
        cycleToNextScreen();
    }
    
    if (pendingStreamChange) {
        pendingStreamChange = false;
        nextStream();
    }
    
    if (pendingPrevStream) {
        pendingPrevStream = false;
        prevStream();
    }
    
    // Если запланирована отложенная смена станции — проверяем истечение debounce
  #if ENABLE_STATION_SELECT_DEBOUNCE
    if (pendingSelectedStreamIndex >= 0 && pendingSelectRequestTime > 0 && (millis() - pendingSelectRequestTime) >= STATION_SELECT_DEBOUNCE_MS) {
      int idx = pendingSelectedStreamIndex;
      pendingSelectedStreamIndex = -1;
      pendingSelectRequestTime = 0;
      selectStreamNow(idx);
    }
  #else
    if (pendingSelectedStreamIndex >= 0) {
      int idx = pendingSelectedStreamIndex;
      pendingSelectedStreamIndex = -1;
      pendingSelectRequestTime = 0;
      selectStreamNow(idx);
    }
  #endif
    serviceWebBackgroundTasks();
    irRemoteLoop();
    
    handleVolumeEncoderRotation();
    handleNavEncoderRotation();
    handleLedControlButtons();

    sleepTimer.update();

    // Обработка OneButton (если используется)
    if (encoderBtn) encoderBtn->tick();
    if (navEncoderBtn) navEncoderBtn->tick();

    checkEncoderLongPress();
    checkNavEncoderLongPress();
    if (pendingNavEncoderLongPress) {
        pendingNavEncoderLongPress = false;
        if (showStationList) {
            closeStationList(true);
        } else {
            openPlaylistList();
        }
    }
    if (pendingEncoderLongPress) {
        pendingEncoderLongPress = false;
        if (showStationList) {
            closeStationList(true);
        } else {
            openPlaylistList();
        }
    }
    if (!kHasNavEncoder && showStationList && pendingEncoderClick) {
        pendingEncoderClick = false;
        pendingEncoderClicks = 0;
        lastClickTime = 0;
        selectStationListItem();
        return;
    }
    if (!kHasNavEncoder && pendingEncoderClick && (millis() - lastClickTime > CLICK_TIMEOUT)) {
        pendingEncoderClick = false;

        if (pendingEncoderClicks == 1) {
            nextStream();
        } else if (pendingEncoderClicks >= 2) {
            prevStream();
        }

        pendingEncoderClicks = 0;
    }
    handleNavEncoderButton();
    
    if (sleepTimer.shouldSleep()) {
        if (radioStarted) {
            audio.stopSong();
            radioStarted = false;
            isPaused = false;
        }
        tftBacklightOff();
        //Serial.println("Sleep timer activated - display off");
        redrawScreen();
    }
    
    static bool wasSleeping = false;
    if (sleepTimer.isSleepMode()) {
        if (flagSleepTimer) {
            flagSleepTimer = false;
            tftBacklightOn();
            sleepTimer.wakeUp();
            if (streamCount > 0 && wifiConnected) {
                startRadio();
            }
            //Serial.println("Woke up from sleep timer");
            redrawScreen();
        }
        wasSleeping = true;
        delay(100);
        return;
    } else if (wasSleeping) {
        wasSleeping = false;
        noteUserActivity();
        redrawScreen();
    }

    serviceScreensaver();
    
    if (metadataPending && (millis() - metadataRequestTime > METADATA_TIMEOUT)) {
        //Serial.println("Metadata timeout - proceeding without metadata");
        metadataPending = false;
        redrawScreen();
    }

    if (audioInfoRefreshPending) {
        audioInfoRefreshPending = false;
        redrawScreen();
    }

    servicePendingStreamStateSave();
    servicePendingVolumeSave();
    
    if (pendingToggle) {
        pendingToggle = false;
        noteUserActivity();
        
        if (radioStarted && !isPaused) {
            audio.pauseResume();
            isPaused = true;
            //Serial.println("Radio paused");
        } else if (radioStarted && isPaused) {
            audio.pauseResume();
            isPaused = false;
            //Serial.println("Radio resumed");
        } else {
            startRadio();
        }
        
        redrawScreen();
    }
    
    static unsigned long lastDiag = 0;
    if (millis() - lastDiag > 60000) {
        lastDiag = millis();
        //Serial.printf("=== DIAGNOSTICS ===\n");
        //Serial.printf("Radio: %s, Paused: %s\n", radioStarted ? "ON" : "OFF", isPaused ? "YES" : "NO");
        //Serial.printf("InBuffer filled: %d KB / %d KB\n", audio.inBufferFilled() / 1024, audio.getInBufferSize() / 1024);
        //Serial.printf("Free heap: %d KB\n", ESP.getFreeHeap() / 1024);
        //Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
        //Serial.printf("WiFi signal: %d dBm\n", WiFi.RSSI());
        if (radioStarted && !isPaused) {
            //Serial.printf("Bitrate: %d kbps\n", audio.getBitRate() / 1000);
        }
        //Serial.printf("Streams in playlist: %d\n", streamCount);
        //Serial.printf("LittleFS free: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
    }
    
    if (radioStarted && !isPaused && !isWebFileTransferActive()) {
        audio.loop();
        // delay(1);
    }
    // В loop(), после audio.loop() (примерно в том месте, где обновляется VU)
    #ifdef WS2812B_PIN
      if (ledMusicEnabled) {
        int vu = (radioStarted && !isPaused) ? audio.getVUlevel() : 0;
        // Используйте один из режимов:
        ledMusic.update(vu, radioStarted && !isPaused, isPaused);      // Режим 1: плавное изменение цвета
        // ledMusic.updatePulse(vu, radioStarted && !isPaused, isPaused); // Режим 2: пульсация красным
        // ledMusic.updateMeter(vu, radioStarted && !isPaused, isPaused);  // Режим 3: как VU метр (зеленый->желтый->красный)
      } else {
        ledMusic.off();
      }
    #endif
    if (flagVolUp) {
        flagVolUp = false;
        handleVolumeStep(1);
    }
    
    if (flagVolDown) {
        flagVolDown = false;
        handleVolumeStep(-1);
    }
    
    if (flagNext) {
        flagNext = false;
        nextStream();
        //Serial.println("Next station");
    }
    
    if (flagPrev) {
        flagPrev = false;
        prevStream();
        //Serial.println("Previous station");
    }
    
    if (flagPlayPause) {
        flagPlayPause = false;
        pendingToggle = true;
        //Serial.println("Toggle play/pause");
    }
    
    if (flagSleepTimer) {
        flagSleepTimer = false;
        if (sleepTimer.isSleepMode()) {
            //Serial.println("=== WAKE UP BUTTON PRESSED ===");
            tftBacklightOn();
            //Serial.println("Backlight ON");
            sleepTimer.wakeUp();
            if (streamCount > 0 && wifiConnected) {
                //Serial.println("Restarting radio...");
                audio.stopSong();
                radioStarted = false;
                isPaused = false;
                icy_station = "";
                icy_streamtitle = "";
                icy_bitrate = "";
                audio_codec = "";
                audio_frequency = "";
                audio_bitness = "";
                delay(100);
                startRadio();
                //Serial.println("Radio restarted");
            }
            redrawScreen();
            //Serial.println("System fully woken up");
        }
    }
    
    checkStreamChange();
    delay(10);
}
