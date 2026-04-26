// ============================================
// Sleep Timer для ESP32 Radio
// sleep_timer.h
// ============================================

#ifndef SLEEP_TIMER_H
#define SLEEP_TIMER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include "config.h"

class SleepTimer {
private:
    unsigned long sleepMillis = 0;     // Когда перейти в сон (в миллисекундах)
    unsigned long sleepDuration = 0;   // Длительность сна в минутах
    bool isActive = false;
    bool blinkState = false;
    unsigned long lastBlinkTime = 0;
    const unsigned long BLINK_INTERVAL = 500; // 0.5 секунды

    // Координаты для отображения индикатора
    int indicatorX = SLEEP_INDICATOR_X;
    int indicatorY = SLEEP_INDICATOR_Y;
    int indicatorRadius = 6;

    // Флаг режима сна
    bool isSleeping = false;

    // Флаг необходимости перерисовки
    bool needsRedraw = true;

public:
    SleepTimer() {
        //Serial.printf("Sleep Timer initialized at (%d, %d)\n", indicatorX, indicatorY);
    }

    // Конструктор с возможностью переопределить координаты
    SleepTimer(int x, int y) : indicatorX(x), indicatorY(y) {
        //Serial.printf("Sleep Timer initialized at custom position (%d, %d)\n", indicatorX, indicatorY);
    }

    // Установить таймер сна
    void setTimer(int minutes) {
        if (minutes == 0) {
            // Отключить таймер
            isActive = false;
            sleepDuration = 0;
            sleepMillis = 0;
            //Serial.println("Sleep timer OFF");
        } else {
            // Включить таймер
            sleepDuration = minutes;
            sleepMillis = millis() + (minutes * 60 * 1000UL);
            isActive = true;
            //Serial.printf("Sleep timer ON: %d minutes\n", minutes);
        }
        needsRedraw = true;
    }

    // Получить оставшееся время в минутах
    int getRemainingMinutes() {
        if (!isActive) return 0;

        unsigned long remaining = (sleepMillis > millis())
                                 ? (sleepMillis - millis()) / 60000
                                 : 0;
        return (int)remaining;
    }

    // Проверить, пора ли спать
    bool shouldSleep() {
        if (!isActive) return false;

        if (millis() >= sleepMillis) {
            //Serial.println("Sleep timer expired - going to sleep");
            isActive = false;
            isSleeping = true;
            needsRedraw = true;
            return true;
        }
        return false;
    }

    // Пробудить систему
    void wakeUp() {
        isSleeping = false;
        needsRedraw = true;
        //Serial.println("Wake up from sleep");
    }

    // Проверить, спит ли система
    bool isSleepMode() const {
        return isSleeping;
    }

    // Обновить состояние мигания
    void update() {
        if (isSleeping || !isActive) {
            return; // В режиме сна или при неактивном таймере не мигаем
        }

        unsigned long now = millis();
        if (now - lastBlinkTime >= BLINK_INTERVAL) {
            blinkState = !blinkState;
            lastBlinkTime = now;
            needsRedraw = true;
        }
    }

    // Нарисовать индикатор на экране
    void drawIndicator(Arduino_Canvas* bgSprite, Arduino_GFX* gfx) {
        if (!needsRedraw) {
            return;
        }

        if (isSleeping) {
            // В режиме сна - красный кружок без мигания
            gfx->fillCircle(indicatorX, indicatorY, indicatorRadius, RGB565_RED);
            needsRedraw = false;
            return;
        }

        if (!isActive) {
            // Если таймер не активен - очищаем область
            int clearRadius = indicatorRadius + 2;
            if (bgSprite) {
                bgSprite->fillCircle(indicatorX, indicatorY, clearRadius, RGB565_BLACK);
            }
            gfx->fillCircle(indicatorX, indicatorY, clearRadius, RGB565_BLACK);
            needsRedraw = false;
            return;
        }

        // Активный таймер - мигающий зеленый кружок
        int clearRadius = indicatorRadius + 2;
        if (bgSprite) {
            bgSprite->fillCircle(indicatorX, indicatorY, clearRadius, RGB565_BLACK);
        }
        gfx->fillCircle(indicatorX, indicatorY, clearRadius, RGB565_BLACK);

        if (blinkState) {
            gfx->fillCircle(indicatorX, indicatorY, indicatorRadius, RGB565_GREEN);
        }

        needsRedraw = false;
    }

    // Установить координаты индикатора
    void setIndicatorPosition(int x, int y) {
        indicatorX = x;
        indicatorY = y;
        needsRedraw = true;
        //Serial.printf("Sleep timer position changed to (%d, %d)\n", indicatorX, indicatorY);
    }

    // Получить координаты индикатора
    int getIndicatorX() const { return indicatorX; }
    int getIndicatorY() const { return indicatorY; }
    int getIndicatorRadius() const { return indicatorRadius; }

    // Получить статус
    bool isTimerActive() const {
        return isActive;
    }

    // Получить текущую установленную длительность
    int getSleepDuration() const {
        return sleepDuration;
    }

    // Сбросить таймер
    void reset() {
        isActive = false;
        sleepMillis = 0;
        sleepDuration = 0;
        isSleeping = false;
        needsRedraw = true;
    }

    // Сохранить состояние в Preferences
    void saveState(Preferences& prefs) {
        prefs.putULong("sleepMillis", sleepMillis);
        prefs.putUInt("sleepDuration", sleepDuration);
        prefs.putBool("sleepActive", isActive);
        prefs.putBool("isSleeping", isSleeping);
    }

    // Загрузить состояние из Preferences
    void loadState(Preferences& prefs) {
        sleepMillis = prefs.getULong("sleepMillis", 0);
        sleepDuration = prefs.getUInt("sleepDuration", 0);
        isActive = prefs.getBool("sleepActive", false);
        isSleeping = prefs.getBool("isSleeping", false);

        // Если таймер был активен, но время уже истекло
        if (isActive && sleepMillis > 0 && millis() >= sleepMillis) {
            isSleeping = true;
            isActive = false;
        }

        needsRedraw = true;
        //Serial.printf("Sleep Timer loaded: active=%d, sleeping=%d\n", isActive, isSleeping);
    }
};

#endif // SLEEP_TIMER_H
