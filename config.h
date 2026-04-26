// ============================================
// ESP32 Radio Configuration File
// config.h
// ============================================

#ifndef CONFIG_H
#define CONFIG_H

#include "fonts/fonts.h"

// === WIFI DEFAULT CREDENTIALS ===
// Эти данные будут использоваться, если в памяти нет сохраненных настроек.
#define DEFAULT_WIFI_SSID     "Tibor"        // Замените на имя вашей WiFi сети
#define DEFAULT_WIFI_PASSWORD "fox25011970"  // Замените на пароль вашей WiFi сети

// === TFT DISPLAY TYPE ===
// Uncomment ONE display type:
//#define DISPLAY_PROFILE_CUSTOM_GENERATED  // use generated profile from profile_custom_generated.h
//#define DISPLAY_ST7796
//#define DISPLAY_NV3007
//#define DISPLAY_ST7735_160x128  // для ST7735 160x128
//#define DISPLAY_ST7789
//#define DISPLAY_ILI9341
#define DISPLAY_ILI9488
//#define DISPLAY_ST7789_172      // горизонтально: TFT_ROTATION 3, вертикально: TFT_ROTATION 0 или 2
//#define DISPLAY_ST7789_76

#define TFT_ROTATION 3        // горизонтально: 1 или 3, вертикально: 0 или 2
#define TFT_BRIGHTNESS 255    // 0-255
// Set to 1 if your backlight control is active-low (inverted).
#define TFT_BL_INVERTED 0     // 0 - normal, 1 - inverted (284/76)

// === TFT DISPLAY PINS ===
#define TFT_DC   9
#define TFT_CS   10
#define TFT_RST  -1
#define TFT_BL   14
#define TFT_MOSI 11
#define TFT_SCLK 12

#define ENCODER_A_PIN     4
#define ENCODER_B_PIN     5
#define ENCODER_BTN_PIN   6

// Режимы работы энкодера:
//
// 1) Режим одного энкодера: установите контакты A/B NAV-энкодера в 255.
//    Вращение = регулировка громкости
//    Одинарное нажатие = следующий трек
//    Двойное нажатие = предыдущий трек
//    Долгое нажатие = следующий экран
//
// 2) Режим двух энкодеров: укажите реальные контакты A/B для NAV-энкодера.
//    Энкодер 1 = громкость + кнопка play/pause
//    Энкодер 2 = навигация по станциям + кнопка смены экрана

#define NAV_ENCODER_A_PIN   255
#define NAV_ENCODER_B_PIN   255
#define NAV_ENCODER_BTN_PIN 255

#define ENCODER_STEPS 4

// === ENCODER BUTTON SETTINGS ===
#define ENCODER_BTN_DEBOUNCE 50  // ms - антидребезг кнопки
#define LONG_PRESS_TIME 2000     // ms - время для длинного нажатия
#define CLICK_TIMEOUT 500        // ms - таймаут между кликами

#define BTN_VOL_UP_PIN     255
#define BTN_VOL_DOWN_PIN   255
#define BTN_NEXT_PIN       255
#define BTN_PREV_PIN       255
#define BTN_PLAY_PAUSE_PIN 255

// === IR REMOTE ===
// Set the IR receiver GPIO here. Leave 255 to disable IR control.
#define IR_REMOTE_PIN 255

// Common 21-key NEC/Panasonic remote defaults.
// Change these values if your remote sends other command codes.
#define IR_CMD_VOL_UP      0x40
#define IR_CMD_VOL_DOWN    0x5
#define IR_CMD_PLAY_PAUSE  0x3
#define IR_CMD_NEXT_STREAM 0x1
#define IR_CMD_PREV_STREAM 0x41
// #define IR_CMD_NEXT_SCREEN   0x4444
// #define IR_CMD_NEXT_PLAYLIST 0x4445

// === AUDIO ===
#define AUDIO_I2S_BCLK 16   // I2S bit clock
#define AUDIO_I2S_LRCLK 18  // I2S left/right clock
#define AUDIO_I2S_DOUT 17   // I2S data out

#define AUDIO_DEFAULT_VOLUME 12  // 0-21
// #define AUDIO_BUFFER_SIZE (512 * 1024) // 512 KB

// === SPRITES ===
#define SPRITE_WIDTH 140   // ширина спрайта стрелок
#define SPRITE_HEIGHT 40   // высота спрайта стрелок

// #define PLAYLIST_PIN 45  // пин для активации плейлиста

// === SLEEP TIMER ===
#define SLEEP_TIMER_BUTTON_PIN 255  // кнопка для таймера сна
#define SLEEP_INDICATOR_X 5         // координаты индикатора
#define SLEEP_INDICATOR_Y 5

#define PLAYLIST_BUTTON_PIN 255     // пин для выбора плейлиста

// === VU NEEDLE LEVELS ===
#define NEEDLE_MIN_VALUE 10   // Minimum VU value
#define NEEDLE_MAX_VALUE 180  // Maximum VU value

// === DISPLAY PROFILE DEFAULTS ===
#include "display_profiles/display_profile.h"

// === FONT CONFIG ===
#define CLOCK_FONT DirectiveFour40
#define STATION_FONT_SMALL consolabUkr8
#define STATION_FONT_NORMAL consolabUkr16
#define STATION_FONT_LARGE consolabUkr36
#define BITRATE_FONT consolabUkr8
#define QUOTE_FONT  consolabUkr16

// === DISPLAY UPDATE ===
#define DISPLAY_TASK_DELAY 60   // milliseconds (25 FPS)
#define CLOCK_UPDATE_DELAY 500  // milliseconds

// === NTP TIME ===
#define NTP_SERVER_1 "time.ntp.org.ua"
#define NTP_SERVER_2 "pool.ntp.org.ua"
#define TIMEZONE_OFFSET 3       // UTC+3 (in hours)

// === DEBUG ===
// === DISPLAY OPTIONS ===
#define SHOW_SECONDS false  // true - показывать секунды, false - только часы и минуты
#define BLINK_COLON true    // true - мигающее двоеточие, false - статичное
#define DEBUG_BORDERS true  // показать рамку спрайта стрелок
#define SERIAL_BAUD 115200

// === WS2812B LED STRIP ===
#define WS2812B_PIN 48
#define WS2812B_LED_COUNT 1
// #define WS2812B_BRIGHTNESS 100  // яркость 0-255
#define LED_UPDATE_INTERVAL 30    // интервал обновления в мс

#endif // CONFIG_H
