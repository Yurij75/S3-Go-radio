#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Audio.h>
#include <TJpg_Decoder.h>
#include <Preferences.h>
#include "time.h"
#include <LittleFS.h>
#include <IRremote.hpp>
#include "led_music.h"
#include "config.h"
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
#include <ArduinoJson.h>

#include "sleep_timer.h"
#include "yoEncoder.h"

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
bool ledMusicEnabled = true;
bool vuMeterEnabled = true;
bool primaryMarqueeEnabled = true;
bool secondaryMarqueeEnabled = false;
int ledStripCount = WS2812B_LED_COUNT;
int ledEffectIndex = 0;
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

// --- Clock ---
String prevClock = "--:--:--";
String prevBitrate = "";
bool redrawClock = true;
String prevStreamName = "";

// === Playlist Files List Display ===
bool showPlaylistsList = false;
unsigned long playlistsListStartTime = 0;
const unsigned long PLAYLISTS_DISPLAY_TIMEOUT = 5000; // 5 секунд
int playlistsScrollIndex = 0;
std::vector<String> playlistFiles; // Вектор с именами файлов плейлистов
int selectedPlaylistIndex = 0; // Индекс выбранного плейлиста в списке

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
bool quoteMarqueeEnabled = false;
int quoteX = 20;
int quoteY = 210;
int quoteW = 0;
String quoteApiUrl = "http://api.forismatic.com/api/1.0/?method=getQuote&format=json&lang=ru";
int vuBarsHeight = 34;
int vuBarsSegments = 15;
int vuBarsOffsetX = 0;
int vuBarsOffsetY = 0;
int vuCardioX = 8;
int vuCardioY = 92;
int vuCardioW = 304;
int vuCardioH = 104;

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

// Добавьте где-то с другими volatile флагами (рядом с flagVolUp и др.)
volatile bool flagPlaylistButton = false;

volatile unsigned long lastVolUpISR = 0;
volatile unsigned long lastVolDownISR = 0;
volatile unsigned long lastNextISR = 0;
volatile unsigned long lastPrevISR = 0;
volatile unsigned long lastPlayPauseISR = 0;

const unsigned long DEBOUNCE_MS = 100; // Уменьшили для кнопок

// === Rotary Encoder ===
yoEncoder* encoder = nullptr;
yoEncoder* navEncoder = nullptr;
volatile unsigned long lastEncoderRotationAt = 0;
volatile bool encoderBtnPressed = false;
volatile unsigned long lastEncoderBtnPress = 0;
volatile unsigned long lastNavEncoderRotationAt = 0;
volatile bool navEncoderBtnPressed = false;
volatile unsigned long lastNavEncoderBtnPress = 0;
volatile unsigned long navEncoderBtnPressStart = 0;
volatile unsigned long encoderBtnPressStart = 0; // Добавьте эту переменную

// Флаги для обработки в основном цикле
bool pendingEncoderClick = false;
bool pendingEncoderLongPress = false;
int pendingEncoderClicks = 0;
bool pendingNavEncoderClick = false;
unsigned long lastClickTime = 0; // Добавьте для подсчета кликов

// Вращение энкодера
volatile long lastEncoderValue = 0;
volatile long currentEncoderValue = 0;
volatile long lastNavEncoderValue = 0;
bool displaySettingsSavePending = false;
bool streamStateSavePending = false;
bool audioInfoRefreshPending = false;
unsigned long streamStateSaveAt = 0;
const unsigned long STREAM_STATE_SAVE_DELAY_MS = 3000;
const unsigned long ENCODER_BUTTON_ROTATION_GUARD_MS = 150;
const unsigned long ENCODER_MIN_VALID_PRESS_MS = 30;
constexpr bool kHasNavEncoder = (NAV_ENCODER_A_PIN != 255 && NAV_ENCODER_B_PIN != 255);
constexpr bool kHasNavEncoderButton = (NAV_ENCODER_BTN_PIN != 255);

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
void scheduleStreamStateSave();
void servicePendingStreamStateSave();
void loadSettings();
void applyAudioEq();
bool shouldShowMetadataScreen();
void handleVolumeEncoderRotation();
void handleNavEncoderRotation();
void handleNavEncoderButton();
void handleAPIAudioInfo();
void handleIrRemote();
// === Forward Declarations для таймера сна ===
void handleSleepTimer();
void handleAPISleepTimer();
void handleVolumeStep(int delta);
bool activatePlaylistByIndex(int idx);
bool selectNextPlaylist();
void cycleToNextScreen();
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

// === Load Playlist Files ===
void loadPlaylistFiles() {
    playlistFiles.clear();
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    while(file) {
        String name = String(file.name());
        if(name.endsWith(".csv")) {
            playlistFiles.push_back(name);
        }
        file = root.openNextFile();
    }
    
    //Serial.printf("Found %d playlist files\n", playlistFiles.size());
    
    // Находим индекс текущего плейлиста в списке
    selectedPlaylistIndex = 0;
    for(int i = 0; i < (int)playlistFiles.size(); i++) {
        if(playlistFiles[i] == currentPlaylistFile) {
            selectedPlaylistIndex = i;
            break;
        }
    }
    
    playlistsScrollIndex = selectedPlaylistIndex;
}

// === Draw Playlists List ===
// === Draw Playlists List ===
void drawPlaylistsList() {
    if (!showPlaylistsList || playlistFiles.size() == 0) return;
    
    // Очищаем область (на всю высоту минус небольшой отступ сверху и снизу)
    int listX = 5;
    int listY = 5;
    int listWidth = vuWidth - 10;
    int listHeight = vuHeight - 10;
    
    // Полупрозрачный темный фон
    bgSprite->fillRect(listX, listY, listWidth, listHeight, RGB565(20, 20, 20)); // Очень темно-серый
    bgSprite->drawRect(listX, listY, listWidth, listHeight, RGB565_GRAY);
    
    // Список плейлистов
    bgSprite->setFont(&STATION_FONT_NORMAL);
    
    int itemHeight = 25;
    int itemsPerPage = (listHeight - 20) / itemHeight; // Автоматический расчет количества элементов
    
    // Ограничиваем прокрутку
    if(playlistsScrollIndex < 0) playlistsScrollIndex = 0;
    int maxScrollIndex = std::max(0, (int)playlistFiles.size() - itemsPerPage);
    if(playlistsScrollIndex > maxScrollIndex) {
        playlistsScrollIndex = maxScrollIndex;
    }
    
    // Убедимся, что выбранный элемент виден
    if(selectedPlaylistIndex < playlistsScrollIndex) {
        playlistsScrollIndex = selectedPlaylistIndex;
    } else if(selectedPlaylistIndex >= playlistsScrollIndex + itemsPerPage) {
        playlistsScrollIndex = selectedPlaylistIndex - itemsPerPage + 1;
    }
    
    int startY = listY + 20;
    
    for(int i = 0; i < itemsPerPage && (i + playlistsScrollIndex) < playlistFiles.size(); i++) {
        int idx = playlistsScrollIndex + i;
        String fileName = playlistFiles[idx];
        fileName = fileName.substring(1); // Убираем "/"
        
        // Обрезаем длинные имена
        if(fileName.length() > 24) {
            fileName = fileName.substring(0, 21) + "...";
        }
        
        // Выделяем выбранный элемент
        if(idx == selectedPlaylistIndex) {
            // Фон для выбранного элемента
            bgSprite->fillRect(listX + 10, startY + i * itemHeight - 15, 
                              listWidth - 20, itemHeight - 5, RGB565_DARKGREEN);
            bgSprite->setTextColor(RGB565_WHITE);
        } else if(fileName + ".csv" == currentPlaylistFile.substring(1)) {
            // Текущий активный плейлист (но не выбранный в списке)
            bgSprite->setTextColor(RGB565_YELLOW);
        } else {
            bgSprite->setTextColor(RGB565_LIGHTGRAY);
        }
        
        // Только имя файла, без номера
        bgSprite->setCursor(listX + 15, startY + i * itemHeight);
        printDisplayText(bgSprite, fileName);
    }
    
    // Индикатор прокрутки (если элементов больше, чем помещается)
    if(playlistFiles.size() > itemsPerPage) {
        int scrollBarHeight = listHeight - 40;
        int scrollBarWidth = 4;
        int scrollBarX = listX + listWidth - 10;
        int scrollBarY = startY;
        
        // Фон полосы прокрутки
        bgSprite->fillRect(scrollBarX, scrollBarY, scrollBarWidth, scrollBarHeight, RGB565_DARKBLUE);
        
        // Ползунок
        int sliderHeight = scrollBarHeight * itemsPerPage / playlistFiles.size();
        if(sliderHeight < 8) sliderHeight = 8; // Минимальная высота ползунка
        
        int sliderY = scrollBarY + (scrollBarHeight - sliderHeight) * playlistsScrollIndex / 
                     std::max(1, (int)playlistFiles.size() - itemsPerPage);
        bgSprite->fillRect(scrollBarX, sliderY, scrollBarWidth, sliderHeight, RGB565_CYAN);
    }
    
    // Нижняя строка с инструкцией и таймером (очень компактно)
    int bottomLineY = listY + listHeight - 15;
    
    // Левая часть - простая инструкция
    bgSprite->setTextColor(RGB565_ORANGE);
    bgSprite->setCursor(listX + 10, bottomLineY);
    printDisplayText(bgSprite, "Rotate:Scroll Click:Select");
    
    // Правая часть - таймер автоскрытия
    unsigned long remaining = 0;
    if(playlistsListStartTime > 0) {
        remaining = (playlistsListStartTime + PLAYLISTS_DISPLAY_TIMEOUT - millis()) / 1000;
        if(remaining < 0) remaining = 0;
    }
    
    if(remaining > 0) {
        bgSprite->setTextColor(RGB565_RED);
        bgSprite->setCursor(listX + listWidth - 60, bottomLineY);
        printDisplayText(bgSprite, String(remaining) + "s");
    }
    
    // Обновляем экран
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t*)bgSprite->getFramebuffer(), vuWidth, vuHeight);
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
void IRAM_ATTR playlistButtonISR() {
    unsigned long now = millis();
    static unsigned long lastPress = 0;
    
    // Антидребезг
    if(now - lastPress < 300) return;
    lastPress = now;
    
    // Только нажатие (FALLING)
    if(digitalRead(PLAYLIST_BUTTON_PIN) == LOW) {  // ИСПРАВЛЕНО: используем PLAYLIST_BUTTON_PIN
        flagPlaylistButton = true;
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
        navEncoderBtnPressed = false;
        navEncoderBtnPressStart = 0;
        pendingNavEncoderClick = true;
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
  prefs.putBool("ledEnabled", ledMusicEnabled);
  prefs.putBool("vuMeterEnabled", vuMeterEnabled);
  prefs.putBool(PREF_KEY_PRIMARY_MARQUEE, primaryMarqueeEnabled);
  prefs.putBool(PREF_KEY_SECONDARY_MARQUEE, secondaryMarqueeEnabled);
  prefs.putInt("ledCount", ledStripCount);
  prefs.putInt("ledEffect", ledEffectIndex);
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
  prefs.putBool("quoteEnabled", quoteMarqueeEnabled);
  prefs.putInt("quoteX", quoteX);
  prefs.putInt("quoteY", quoteY);
  prefs.putInt("quoteW", quoteW);
  prefs.putInt("stationW", stationScrollWidth);
  prefs.putInt("title1W", title1ScrollWidth);
  prefs.putInt("title2W", title2ScrollWidth);
  prefs.putInt("marqueePauseMs", marqueePauseMs);
  prefs.putString("quoteUrl", quoteApiUrl);
  prefs.putInt("vuBarsHeight", vuBarsHeight);
  prefs.putInt("vuBarsSegments", vuBarsSegments);
  prefs.putInt("vuBarsOffsetX", vuBarsOffsetX);
  prefs.putInt("vuBarsOffsetY", vuBarsOffsetY);
  prefs.putInt("vuCardioX", vuCardioX);
  prefs.putInt("vuCardioY", vuCardioY);
  prefs.putInt("vuCardioW", vuCardioW);
  prefs.putInt("vuCardioH", vuCardioH);
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

void loadSettings() {
  prefs.begin("radio", true);
  currentPlaylistFile = prefs.getString("playlist", "/playlist.csv");
  currentPlayingPlaylistFile = prefs.getString("playingPlaylist", currentPlaylistFile);
  currentStreamIndex = prefs.getInt("stream", 0);
  currentVolume = AUDIO_DEFAULT_VOLUME;
  ledMusicEnabled = prefs.getBool("ledEnabled", true);
  vuMeterEnabled = prefs.getBool("vuMeterEnabled", true);
  primaryMarqueeEnabled = prefs.isKey(PREF_KEY_PRIMARY_MARQUEE)
                              ? prefs.getBool(PREF_KEY_PRIMARY_MARQUEE, true)
                              : true;
  secondaryMarqueeEnabled = prefs.isKey(PREF_KEY_SECONDARY_MARQUEE)
                                ? prefs.getBool(PREF_KEY_SECONDARY_MARQUEE, false)
                                : false;
  ledStripCount = prefs.getInt("ledCount", WS2812B_LED_COUNT);
  ledEffectIndex = prefs.getInt("ledEffect", 0);
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
  quoteMarqueeEnabled = prefs.getBool("quoteEnabled", false);
  quoteX = prefs.getInt("quoteX", 20);
  quoteY = prefs.getInt("quoteY", 210);
  quoteW = prefs.getInt("quoteW", 0);
  stationScrollWidth = prefs.getInt("stationW", 0);
  title1ScrollWidth = prefs.getInt("title1W", 0);
  title2ScrollWidth = prefs.getInt("title2W", 0);
  marqueePauseMs = constrain(prefs.getInt("marqueePauseMs", 1500), 0, 10000);
  quoteApiUrl = prefs.getString("quoteUrl", "http://api.forismatic.com/api/1.0/?method=getQuote&format=json&lang=ru");
  vuBarsHeight = constrain(prefs.getInt("vuBarsHeight", 34), 6, 120);
  vuBarsSegments = constrain(prefs.getInt("vuBarsSegments", 15), 4, 40);
  vuBarsOffsetX = constrain(prefs.getInt("vuBarsOffsetX", 0), -200, 200);
  vuBarsOffsetY = constrain(prefs.getInt("vuBarsOffsetY", 0), -200, 200);
  vuCardioX = prefs.getInt("vuCardioX", 8);
  vuCardioY = prefs.getInt("vuCardioY", 92);
  vuCardioW = constrain(prefs.getInt("vuCardioW", 304), 80, 480);
  vuCardioH = constrain(prefs.getInt("vuCardioH", 104), 40, 320);
  prefs.end();
  if (gfx) gfx->setRotation(tftRotation);
  loadLastActiveScreen();
  loadCurrentScreenSettings();
  //Serial.println("✅ Settings loaded");
}

void applyAudioEq() {
  audio.setBalance(static_cast<float>(eqBalance));
  audio.setTone(static_cast<float>(eqBass), static_cast<float>(eqMid), static_cast<float>(eqTreble));
}

// === WiFi ===
bool connectToWiFi() {
  // Load saved WiFi credentials
  Preferences prefs;
  prefs.begin("radio", true);
  String saved_ssid = prefs.getString("wifi_ssid", "");
  String saved_pass = prefs.getString("wifi_pass", "");
  prefs.end();
  
  String ssid, password;
  
  // If no saved credentials, use defaults from config.h
  if (saved_ssid.length() == 0) {
    #ifdef DEFAULT_WIFI_SSID
      ssid = DEFAULT_WIFI_SSID;
      password = DEFAULT_WIFI_PASSWORD;
      //Serial.println("Using default WiFi credentials from config.h");
    #else
      //Serial.println("No WiFi credentials found - starting setup AP");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("S3-Go!-light-Setup");
      Serial.print("Setup AP IP: ");
      //Serial.println(WiFi.softAPIP());
      wifiConnected = false;
      return false;
    #endif
  } else {
    // Use saved credentials
    ssid = saved_ssid;
    password = saved_pass;
    //Serial.println("Using saved WiFi credentials");
  }
  
  // Try to connect
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.setSleep(false);
  Serial.print("Connecting WiFi: ");
  //Serial.println(ssid.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    
    // Save the default credentials if they were used and not saved before
    if (saved_ssid.length() == 0) {
      Preferences prefsWrite;
      prefsWrite.begin("radio", false);
      prefsWrite.putString("wifi_ssid", ssid);
      prefsWrite.putString("wifi_pass", password);
      prefsWrite.end();
      //Serial.println("Default WiFi credentials saved to memory");
    }
    
    return true;
  } else {
    //Serial.println("\nWiFi connection failed - starting setup AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("S3-Go!-light-Setup");
    Serial.print("Setup AP IP: ");
    //Serial.println(WiFi.softAPIP());
    wifiConnected = false;
    return false;
  }
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
  unsigned long splashDuration = 5000;
  
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
  
  //Serial.printf("✅ Loaded stream #%d: %s -> %s\n", idx, outName.c_str(), outURL.c_str());
  return true;
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
  return getCurrentScreen() == ScreenId::VuMeter1;
}

void selectStream(int idx) {
  if(idx < 0 || idx >= streamCount) return;
  
  currentStreamIndex = idx;
  currentPlayingPlaylistFile = currentPlaylistFile;
  
  if(getStreamByIndex(idx, currentStreamName, currentStreamURL)) {
    // Reset metadata before switching stream
    icy_station = "";
    icy_streamtitle = "";
    icy_bitrate = "";
    audio_codec = "";
    audio_frequency = "";
    audio_bitness = "";
    
    metadataPending = true;
    metadataRequestTime = millis();
    
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

void handleVolumeStep(int delta) {
  const int newVolume = constrain(currentVolume + delta, 0, 50);
  if (newVolume == currentVolume) return;

  currentVolume = newVolume;
  audio.setVolume(currentVolume);
  if (encoder && !showPlaylistsList) {
    encoder->setEncoderValue(currentVolume);
    lastEncoderValue = currentVolume;
  }
  drawVolumeDisplay();
}

bool activatePlaylistByIndex(int idx) {
  loadPlaylistFiles();
  if (playlistFiles.empty()) return false;

  const int playlistCount = static_cast<int>(playlistFiles.size());
  if (idx < 0) {
    idx = (idx % playlistCount + playlistCount) % playlistCount;
  } else if (idx >= playlistCount) {
    idx %= playlistCount;
  }

  selectedPlaylistIndex = idx;
  loadPlaylistMetadata(playlistFiles[selectedPlaylistIndex]);
  if (streamCount > 0) {
    selectStream(0);
  } else {
    savePlaybackState();
  }

  if (showPlaylistsList) {
    showPlaylistsList = false;
    if (navEncoder) {
      navEncoder->setEncoderValue(0);
      lastNavEncoderValue = 0;
    }
    redrawScreen();
  }
  return true;
}

bool selectNextPlaylist() {
  loadPlaylistFiles();
  if (playlistFiles.empty()) return false;
  return activatePlaylistByIndex(selectedPlaylistIndex + 1);
}

void cycleToNextScreen() {
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

void handleIrRemote() {
    static unsigned long lastCommandTime = 0;
    static uint16_t lastCommand = 0;
    
    if (!IrReceiver.decode()) return;
    
    const uint16_t command = IrReceiver.decodedIRData.command;
    const bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);
    const unsigned long now = millis();
    
    // Анти-флуд для навигационных команд
    if (command != IR_CMD_VOL_UP && command != IR_CMD_VOL_DOWN) {
        if (isRepeat && command == lastCommand && (now - lastCommandTime) < 300) {
            IrReceiver.resume();
            return;
        }
        lastCommand = command;
        lastCommandTime = now;
    }
    
    switch (command) {
        case IR_CMD_VOL_UP:
            handleVolumeStep(1);
            break;
        case IR_CMD_VOL_DOWN:
            handleVolumeStep(-1);
            break;
        case IR_CMD_PLAY_PAUSE:
            pendingToggle = true;
            break;
        case IR_CMD_NEXT_STREAM:
            pendingStreamChange = true;
            break;
        case IR_CMD_PREV_STREAM:
            pendingPrevStream = true;
            break;
        // case IR_CMD_NEXT_SCREEN:
        //     pendingScreenChange = true;
        //     break;
        // case IR_CMD_NEXT_PLAYLIST:
        //     selectNextPlaylist();
        //     break;
    }
    
    IrReceiver.resume();
}

void checkStreamChange() {
  if (streamChangeRequested && streamCount > 0) {
    nextStream();
    streamChangeRequested = false;
  }
}


void handleVolumeEncoderRotation() {
    if (!encoder || showPlaylistsList) return;

    long newEncoderValue = encoder->readEncoder();
    if (newEncoderValue == lastEncoderValue) return;

    int diff = static_cast<int>(newEncoderValue - lastEncoderValue);
    lastEncoderRotationAt = millis();
    lastEncoderValue = newEncoderValue;

    if (diff > 0) {
        handleVolumeStep(1);
    } else if (diff < 0) {
        handleVolumeStep(-1);
    }
}

void handleNavEncoderRotation() {
    if (!navEncoder) return;

    long newEncoderValue = navEncoder->readEncoder();
    if (newEncoderValue == lastNavEncoderValue) return;

    long diff = newEncoderValue - lastNavEncoderValue;
    lastNavEncoderRotationAt = millis();

    if (showPlaylistsList) {
        lastNavEncoderValue = newEncoderValue;
        playlistsListStartTime = millis();

        if (diff > 0) {
            while (diff-- > 0 && selectedPlaylistIndex < (int)playlistFiles.size() - 1) {
                selectedPlaylistIndex++;
            }
        } else {
            while (diff++ < 0 && selectedPlaylistIndex > 0) {
                selectedPlaylistIndex--;
            }
        }

        navEncoder->setEncoderValue(selectedPlaylistIndex);
        lastNavEncoderValue = selectedPlaylistIndex;
        drawPlaylistsList();
        return;
    }

    if (diff > 0) {
        while (diff-- > 0) {
            nextStream();
        }
    } else {
        while (diff++ < 0) {
            prevStream();
        }
    }

    navEncoder->setEncoderValue(0);
    lastNavEncoderValue = 0;
}

void handleNavEncoderButton() {
    if (!pendingNavEncoderClick) return;

    pendingNavEncoderClick = false;

    if (showPlaylistsList) {
        String selectedFile = playlistFiles[selectedPlaylistIndex];
        loadPlaylistMetadata(selectedFile);
        if (streamCount > 0) {
            selectStream(0);
        } else {
            savePlaybackState();
        }
        showPlaylistsList = false;

        icy_station = "";
        icy_streamtitle = "";
        icy_bitrate = "";
        audio_codec = "";
        audio_frequency = "";
        audio_bitness = "";

        loadBackground();
        redrawScreen();

        if (navEncoder) {
            navEncoder->setEncoderValue(0);
            lastNavEncoderValue = 0;
        }
        return;
    }

    cycleToNextScreen();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
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
  gfx->begin();
  
  // !!! ВАЖНО: Сразу заполняем черным цветом !!!
  gfx->fillScreen(RGB565_BLACK);
  
  // Устанавливаем подсветку
  initTftBacklight();

  // === File System ===
  if (!LittleFS.begin(false)) {
    //Serial.println("LittleFS mount failed - trying to format...");
    if (LittleFS.begin(true)) {
      //Serial.println("✅ LittleFS formatted and mounted successfully!");
    } else {
      //Serial.println("❌ LittleFS format failed!");
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
  drawBootImageBackground();

  // === Load Settings ===
  initScreenManager();
  loadSettings();

  // === КНОПКИ (БЫЛИ ЗДЕСЬ) ===
  if (BTN_VOL_UP_PIN != 255) pinMode(BTN_VOL_UP_PIN, INPUT_PULLUP);
  if (BTN_VOL_DOWN_PIN != 255) pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);
  if (BTN_NEXT_PIN != 255) pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  if (BTN_PREV_PIN != 255) pinMode(BTN_PREV_PIN, INPUT_PULLUP);
  if (BTN_PLAY_PAUSE_PIN != 255 && BTN_PLAY_PAUSE_PIN != ENCODER_BTN_PIN) {
    pinMode(BTN_PLAY_PAUSE_PIN, INPUT_PULLUP);
  }

  if (SLEEP_TIMER_BUTTON_PIN != 255) {
    pinMode(SLEEP_TIMER_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SLEEP_TIMER_BUTTON_PIN), sleepTimerISR, FALLING);
  }

  if (PLAYLIST_BUTTON_PIN != 255) {
    pinMode(PLAYLIST_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PLAYLIST_BUTTON_PIN), playlistButtonISR, FALLING);
  }
  
  if (BTN_VOL_UP_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_VOL_UP_PIN), volUpISR, FALLING);
  if (BTN_VOL_DOWN_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_VOL_DOWN_PIN), volDownISR, FALLING);
  if (BTN_NEXT_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_NEXT_PIN), nextStreamISR, FALLING);
  if (BTN_PREV_PIN != 255) attachInterrupt(digitalPinToInterrupt(BTN_PREV_PIN), prevStreamISR, FALLING);
  if (BTN_PLAY_PAUSE_PIN != 255 && BTN_PLAY_PAUSE_PIN != ENCODER_BTN_PIN) {
    attachInterrupt(digitalPinToInterrupt(BTN_PLAY_PAUSE_PIN), playPauseISR, FALLING);
  }

  // === Rotary Encoder (ВАЖНО!) ===
  encoder = new yoEncoder(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_STEPS, true);
  encoder->setBoundaries(0, 50, false);
  encoder->setup([]() {
      if (encoder) {
          encoder->readEncoder_ISR();
      }
  });
  encoder->begin();
  encoder->setEncoderValue(currentVolume);
  lastEncoderValue = currentVolume;

  // Кнопка энкодера
  if (ENCODER_BTN_PIN != 255) {
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_BTN_PIN), encoderBtnISR, CHANGE);
  }

  if (kHasNavEncoder) {
    navEncoder = new yoEncoder(NAV_ENCODER_A_PIN, NAV_ENCODER_B_PIN, ENCODER_STEPS, true);
    navEncoder->setBoundaries(-32768, 32767, false);
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
    attachInterrupt(digitalPinToInterrupt(NAV_ENCODER_BTN_PIN), navEncoderBtnISR, CHANGE);
  }

#if IR_REMOTE_PIN != 255
  IrReceiver.begin(IR_REMOTE_PIN, DISABLE_LED_FEEDBACK);
  Serial.printf("[IR] receiver initialized on GPIO %d\n", IR_REMOTE_PIN);
#endif

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
  //Serial.printf("Before getInBufferSize: %d KB\n", audio.getInBufferSize() / 1024);
  applyAudioEq();
  audio.setVolume(currentVolume);
  
    // === LED Music ===
  #ifdef WS2812B_PIN
    if (!ledMusic.begin(ledStripCount)) {
      Serial.println("⚠️ Failed to initialize LED Music");
    } else {
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
    if (kHasNavEncoder) return;
    if (!encoderBtnPressed || encoderBtnPressStart == 0) return;

    unsigned long now = millis();
    unsigned long pressDuration = now - encoderBtnPressStart;
    if (pressDuration < LONG_PRESS_TIME || pendingEncoderLongPress) return;

    pendingEncoderLongPress = true;
    pendingEncoderClick = false;
    pendingEncoderClicks = 0;
    lastClickTime = 0;
    encoderBtnPressStart = 0;
    encoderBtnPressed = false;
}

void loop() {
    server.handleClient();
    
    // ✅ Обработка ИК-команд с высоким приоритетом
    handleIrRemote();
    
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
    serviceWebBackgroundTasks();
    handleIrRemote();
    
    handleVolumeEncoderRotation();
    handleNavEncoderRotation();
    
    if (showPlaylistsList && (millis() - playlistsListStartTime > PLAYLISTS_DISPLAY_TIMEOUT)) {
        showPlaylistsList = false;
        redrawScreen();
        //Serial.println("Playlists list timeout - hiding");
    }
    
    sleepTimer.update();
    
    if (flagPlaylistButton) {
        flagPlaylistButton = false;
        
        if (!showPlaylistsList) {
            showPlaylistsList = true;
            playlistsListStartTime = millis();
            playlistsScrollIndex = 0;
            selectedPlaylistIndex = 0;
            loadPlaylistFiles();
            if (navEncoder) {
                navEncoder->setEncoderValue(selectedPlaylistIndex);
                lastNavEncoderValue = selectedPlaylistIndex;
            }
            drawPlaylistsList();
            //Serial.println("Showing playlists list (Button 4)");
        } else {
            showPlaylistsList = false;
            if (navEncoder) {
                navEncoder->setEncoderValue(0);
                lastNavEncoderValue = 0;
            }
            redrawScreen();
            //Serial.println("Hiding playlists list (Button 4)");
        }
    }
    
    checkEncoderLongPress();
    if (pendingEncoderLongPress) {
        pendingEncoderLongPress = false;
        cycleToNextScreen();
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
        redrawScreen();
    }
    
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
    
    if (pendingToggle) {
        pendingToggle = false;
        
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
      if (ledMusicEnabled && radioStarted) {
        int vu = audio.getVUlevel();
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
