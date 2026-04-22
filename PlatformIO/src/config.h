// ============================================
// ESP32 Radio Configuration File
// config.h
// ============================================

#ifndef CONFIG_H
#define CONFIG_H

// === TFT DISPLAY TYPE ===
// Uncomment ONE display type:
#define DISPLAY_ST7796
//#define DISPLAY_ILI9341
//#define DISPLAY_ST7789
//#define DISPLAY_ST7789_172_3 //горизонтально установить TFT_ROTATION 3
//#define DISPLAY_ST7789_172_0 //вертикальный установить TFT_ROTATION 0 
//#define DISPLAY_ST7789_76_3  //горизонтально установить TFT_ROTATION 3
//#define DISPLAY_ST7789_76_0   //вертикальный установить TFT_ROTATION 0 

#define TFT_ROTATION 0       // 0-3: rotation angle
#define TFT_BRIGHTNESS 255    // 0-255

// === TFT DISPLAY PINS ===
#define TFT_DC   9  
#define TFT_CS   10
#define TFT_RST  -1
#define TFT_BL   14
#define TFT_MOSI 11
#define TFT_SCLK 12

#define BTN_VOL_UP_PIN     4
#define BTN_VOL_DOWN_PIN   5
#define BTN_NEXT_PIN       6
#define BTN_PREV_PIN       7
#define BTN_PLAY_PAUSE_PIN 15

// === AUDIO ===
#define AUDIO_I2S_BCLK 16     // I2S bit clock
#define AUDIO_I2S_LRCLK 18    // I2S left/right clock
#define AUDIO_I2S_DOUT 17     // I2S data out
#define AUDIO_DEFAULT_VOLUME 8 // 0-21
#define AUDIO_BUFFER_SIZE (512 * 1024) // 512 KB

// === SPRITES ===
#define SPRITE_WIDTH 120 // размеры спрайта стрелок
#define SPRITE_HEIGHT 80 // размеры спрайта стрелок

// === VU NEEDLE POSITIONS (default) ===
#define NEEDLE_L_X 22
#define NEEDLE_L_Y 385
#define NEEDLE_R_X 178
#define NEEDLE_R_Y 385

// === VU NEEDLE ANIMATION ===
#define NEEDLE_UP_SPEED 9     // Speed increase
#define NEEDLE_DOWN_SPEED 3    // Speed decrease
#define NEEDLE_MIN_VALUE 10    // Minimum VU value
#define NEEDLE_MAX_VALUE 180   // Maximum VU value

// === VU NEEDLE ANGLES ===
#define NEEDLE_MIN_ANGLE -140   // Минимальный угол
#define NEEDLE_MAX_ANGLE -40    // Максимальный угол

// === TEXT COLORS ===
#define COLOR_STATION 0xFEA0    // Station name (золотистый)
#define COLOR_TITLE 0xFEA0      // Stream title (золотистый)
#define COLOR_BITRATE 0xFEA0    // Bitrate (золотистый)
#define COLOR_VOLUME 0xFEA0     // Volume (золотистый)
#define COLOR_CLOCK 0x07FF      // Clock (cyan)
#define COLOR_NEEDLE_MAIN 0x0000 // Needle main part (black)
#define COLOR_NEEDLE_RED 0xF800  // Needle red part (red)

// === DISPLAY UPDATE ===
#define DISPLAY_TASK_DELAY 40  // milliseconds (25 FPS)
#define CLOCK_UPDATE_DELAY 500 // milliseconds

// === NTP TIME ===
#define NTP_SERVER_1 "time.ntp.org.ua"
#define NTP_SERVER_2 "pool.ntp.org.ua"
#define TIMEZONE_OFFSET 2      // UTC+3 (in hours)

// === DEBUG ===
// === DISPLAY OPTIONS ===
#define SHOW_VOLUME_DISPLAY false  // true - показывать громкость, false - скрыть
#define DEBUG_BORDERS false    // Показать рамку спрайта стрелок -- false -- true --
#define SERIAL_BAUD 115200

#endif // CONFIG_H
