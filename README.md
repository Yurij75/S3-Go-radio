https://s3-go--usb-sd.yurij75maks.workers.dev/
// ============================================
// ESP32 Radio Configuration File
// config.h
// ============================================

#ifndef CONFIG_H
#define CONFIG_H

#include "fonts/fonts.h"

// === ДИНАМИЧЕСКИЙ ПРОФИЛЬ СБОРКИ ИЗ ОБЛАКА ===
#if __has_include("myoptions.h")
    #include "myoptions.h"
#endif

// === WIFI DEFAULT CREDENTIALS ===
#define DEFAULT_WIFI_SSID     "setka"
#define DEFAULT_WIFI_PASSWORD "16031975"

// === TFT DISPLAY TYPE ===
#ifdef WEB_DISPLAY_ST7789
  #define DISPLAY_ST7789
#elif defined(WEB_DISPLAY_ST7789_172)
  #define DISPLAY_ST7789_172
#elif defined(WEB_DISPLAY_ST7789_76)
  #define DISPLAY_ST7789_76
#elif defined(WEB_DISPLAY_ILI9488)
  #define DISPLAY_ILI9488
#elif defined(WEB_DISPLAY_NV3007)
  #define DISPLAY_NV3007
#elif defined(WEB_DISPLAY_ST7735)
  #define DISPLAY_ST7735_160x128
#elif defined(WEB_DISPLAY_ILI9341)
  #define DISPLAY_ILI9341
#elif defined(WEB_DISPLAY_ST7796)
  #define DISPLAY_ST7796
#elif defined(WEB_DISPLAY_NONE)
  #define DISPLAY_PROFILE_CUSTOM_GENERATED
#else
  #ifndef DISPLAY_PROFILE_CUSTOM_GENERATED
    #define DISPLAY_ILI9488
  #endif
#endif

// === TFT ROTATION ===
// 0 = 0°, 1 = 90°, 2 = 180°, 3 = 270°
#ifndef TFT_ROTATION
  #define TFT_ROTATION 3
#endif

#define TFT_BRIGHTNESS 255
#define TFT_BL_INVERTED 0

// === AUDIO (ЦАП, I2S) ===
#ifndef AUDIO_I2S_BCLK
  #define AUDIO_I2S_BCLK 16
#endif
#ifndef AUDIO_I2S_DOUT
  #define AUDIO_I2S_DOUT 17
#endif
#ifndef AUDIO_I2S_LRCLK
  #define AUDIO_I2S_LRCLK 18
#endif
#define AUDIO_DEFAULT_VOLUME 120

// === TFT DISPLAY PINS ===
#ifndef TFT_DC
  #define TFT_DC   9
#endif
#ifndef TFT_CS
  #define TFT_CS   10
#endif
#ifndef TFT_MOSI
  #define TFT_MOSI 11
#endif
#ifndef TFT_SCLK
  #define TFT_SCLK 12
#endif
#ifndef TFT_RST
  #define TFT_RST  -1
#endif
#ifndef TFT_BL
  #define TFT_BL   14
#endif

// === SD CARD ===
#define USE_SD_PLAYER 1
#define SD_CS    39
#define SD_SCK   41
#define SD_MOSI  40
#define SD_MISO  42

// === USB FLASH ===
#define USE_USB_PLAYER 1

// === ENCODER PINS ===
#ifndef ENCODER_A_PIN
  #define ENCODER_A_PIN     4
#endif
#ifndef ENCODER_B_PIN
  #define ENCODER_B_PIN     5
#endif
#ifndef ENCODER_BTN_PIN
  #define ENCODER_BTN_PIN   6
#endif

#ifndef NAV_ENCODER_A_PIN
  #define NAV_ENCODER_A_PIN   255
#endif
#ifndef NAV_ENCODER_B_PIN
  #define NAV_ENCODER_B_PIN   255
#endif
#ifndef NAV_ENCODER_BTN_PIN
  #define NAV_ENCODER_BTN_PIN 255
#endif

#define ENCODER_STEPS 4
#define ENCODER_INTERNAL_PULLUP true
#define NAV_ENCODER_INTERNAL_PULLUP true

#define BTN_VOL_UP_PIN     255
#define BTN_VOL_DOWN_PIN   255
#define BTN_NEXT_PIN       255
#define BTN_PREV_PIN       255
#define BTN_PLAY_PAUSE_PIN 255

// === IR REMOTE ===
#define IR_REMOTE_PIN 255
#define IR_CMD_VOL_UP      0x40
#define IR_CMD_VOL_DOWN    0x5
#define IR_CMD_PLAY_PAUSE  0x3
#define IR_CMD_NEXT_STREAM 0x1
#define IR_CMD_PREV_STREAM 0x41
#define IR_CMD_NEXT_SCREEN   0x0
#define IR_CMD_NEXT_PLAYLIST 0x0

// === SPRITES ===
#define SPRITE_WIDTH 140
#define SPRITE_HEIGHT 40

// === SLEEP TIMER ===
#define SLEEP_TIMER_BUTTON_PIN 255
#define SLEEP_INDICATOR_X 5
#define SLEEP_INDICATOR_Y 5

// === VU NEEDLE ===
#define NEEDLE_MIN_VALUE 10
#define NEEDLE_MAX_VALUE 180

// === DISPLAY PROFILE ===
#include "display_profiles/display_profile.h"

// === FONT CONFIG ===
// 5 независимых позиций шрифтов.
// Для каждой позиции задаётся шрифт по умолчанию (как на скриншоте).
// Если позиция передана через myoptions.h (веб-сборка), макрос там уже
// определён напрямую и блоки ниже - это только резервный fallback.

// 1. Часы
#ifndef CLOCK_FONT
  #ifdef WEB_FONT_CLOCK_DIRECTIVE
    #define CLOCK_FONT DirectiveFour40
  #elif defined(WEB_FONT_CLOCK_CUSTOM)
    #define CLOCK_FONT font_from_custom
  #else
    #define CLOCK_FONT DirectiveFour40
  #endif
#endif

// 2. Имя станции (бегущая строка)
#ifndef STATION_FONT
  #ifdef WEB_FONT_STATION_CONSOLA16
    #define STATION_FONT consolabUkr16
  #elif defined(WEB_FONT_STATION_CUSTOM)
    #define STATION_FONT font_from_custom
  #else
    #define STATION_FONT consolabUkr16
  #endif
#endif

// 3. Исполнитель и трек
#ifndef TRACK_INFO_FONT
  #ifdef WEB_FONT_ARTIST_CONSOLA12
    #define TRACK_INFO_FONT consolabUkr12
  #elif defined(WEB_FONT_ARTIST_CUSTOM)
    #define TRACK_INFO_FONT font_from_custom
  #else
    #define TRACK_INFO_FONT consolabUkr12
  #endif
#endif

// 4. Битрейт / IP
#ifndef INFO_FONT
  #ifdef WEB_FONT_INFO_CONSOLA6
    #define INFO_FONT consolabUkr6
  #elif defined(WEB_FONT_INFO_CUSTOM)
    #define INFO_FONT font_from_custom
  #else
    #define INFO_FONT consolabUkr6
  #endif
#endif

// 5. Плейлист / список станций / технические экраны
#ifndef UI_FONT
  #ifdef WEB_FONT_UI_CONSOLA8
    #define UI_FONT consolabUkr8
  #elif defined(WEB_FONT_UI_CUSTOM)
    #define UI_FONT font_from_custom
  #else
    #define UI_FONT consolabUkr8
  #endif
#endif

// === DISPLAY UPDATE ===
#define DISPLAY_TASK_DELAY 25
#define CLOCK_UPDATE_DELAY 500

// === Station switch debounce ===
#define ENABLE_STATION_SELECT_DEBOUNCE 1
#define STATION_SELECT_DEBOUNCE_MS 500

// === NTP ===
#define NTP_SERVER_1 "time.ntp.org.ua"
#define NTP_SERVER_2 "pool.ntp.org.ua"
#define TIMEZONE_OFFSET 3

// === DEBUG ===
#ifndef SHOW_SECONDS
  #define SHOW_SECONDS false
#endif
#ifndef BLINK_COLON
  #define BLINK_COLON true
#endif
#define DEBUG_BORDERS true
#define SERIAL_BAUD 115200

// === WS2812B ===
#define WS2812B_PIN 48
#define WS2812B_LED_COUNT 1
#define LED_UPDATE_INTERVAL 30
#define LED_TOGGLE_BUTTON_PIN 1
#define LED_EFFECT_BUTTON_PIN 2
#define LED_COLOR_ORDER "GRB"
#define LED_MATRIX_WIDTH 16
#define LED_MATRIX_HEIGHT 16

#endif // CONFIG_H
