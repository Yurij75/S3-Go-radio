#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <vector>
#include <string>
#include <cmath>
#include "config.h"
#include "display_text_utils.h"

const std::vector<std::string> UKRAINE_CITIES = {
    "Київ", "Харків", "Одеса", "Дніпро", "Донецьк", "Запоріжжя", "Львів",
    "Кривий Ріг", "Миколаїв", "Маріуполь", "Вінниця", "Херсон", "Полтава",
    "Чернігів", "Черкаси", "Суми", "Житомир", "Рівне", "Івано-Франківськ",
    "Тернопіль", "Луцьк", "Ужгород", "Кропивницький", "Біла Церква",
    "Кременчук", "Мелітополь", "Сєвєродонецьк", "Чернівці", "Бровари"
};

class CityScreensaver {
private:
    Arduino_GFX* gfx;
    Arduino_Canvas* backBuffer;
    int screenWidth;
    int screenHeight;
    
    struct ActiveCity {
        std::string name;
        int x, y;
        uint8_t alpha;
        bool fadingIn;
        bool holding;     // состояние удержания максимальной яркости
        unsigned long lastUpdate;
        unsigned long holdStartTime; // время начала удержания
        uint16_t lastColor;
        int width;  // Ширина текста в пикселях
        int height; // Высота текста
    };
    
    std::vector<ActiveCity> activeCities;
    std::vector<std::string> customCities;
    unsigned long lastSpawnTime;
    bool enabled;
    bool testMode;
    bool needFullRedraw;
    
    int randomRange(int min, int max) {
        return min + random(max - min + 1);
    }
    
    std::string getRandomCity() {
        if (!customCities.empty()) {
            return customCities[random(0, customCities.size())];
        }
        return UKRAINE_CITIES[random(0, UKRAINE_CITIES.size())];
    }

    void loadCustomCities() {
        customCities.clear();
        if (!LittleFS.exists("/screensaver.txt")) return;

        File file = LittleFS.open("/screensaver.txt", "r");
        if (!file) return;

        while (file.available() && customCities.size() < 80) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (!line.length() || line.startsWith("#")) continue;
            customCities.push_back(std::string(line.c_str()));
        }
        file.close();
    }
    
    // Проверка пересечения с существующими городами
    bool checkCollision(int x, int y, int width, int height, int minDistance = 40) {
        for (const auto& city : activeCities) {
            // Проверяем расстояние между центрами
            int dx = abs(x + width/2 - (city.x + city.width/2));
            int dy = abs(y + height/2 - (city.y + city.height/2));
            int distance = sqrt(dx*dx + dy*dy);
            
            if (distance < minDistance) {
                return true; // Есть коллизия
            }
            
            // Дополнительная проверка на прямоугольное пересечение
            if (!(x + width < city.x || 
                  x > city.x + city.width || 
                  y + height < city.y || 
                  y > city.y + city.height)) {
                return true;
            }
        }
        return false;
    }
    
    // Получить размеры текста
    void getTextDimensions(const std::string& text, int& width, int& height) {
        if (!gfx) return;
        
        int16_t x1, y1;
        uint16_t w, h;
        gfx->setFont(&STATION_FONT_NORMAL);
        getDisplayTextBounds(gfx, text.c_str(), 0, 0, &x1, &y1, &w, &h);
        width = w;
        height = h;
    }
    
    void getRandomPosition(int& x, int& y, int textWidth, int textHeight) {
        int maxAttempts = 50;
        int attempt = 0;
        bool found = false;
        const int minX = 8;
        const int minY = 20;
        const int maxX = max(minX, screenWidth - textWidth - 8);
        const int maxY = max(minY, screenHeight - textHeight - 8);
        
        while (!found && attempt < maxAttempts) {
            x = randomRange(minX, maxX);
            y = randomRange(minY, maxY);
            
            if (!checkCollision(x, y, textWidth, textHeight)) {
                found = true;
            }
            attempt++;
        }
        
        // Если не нашли свободное место, просто ставим в случайное место
        if (!found) {
            x = randomRange(minX, maxX);
            y = randomRange(minY, maxY);
        }
    }
    
    uint16_t getColorForAlpha(uint8_t alpha) {
        uint8_t blue = (alpha * 31) / 255;
        return 0x001F & (blue);
    }
    
public:
    CityScreensaver(Arduino_GFX* display) : gfx(display), backBuffer(nullptr), 
                                             enabled(false), testMode(false), 
                                             needFullRedraw(true) {
        if (gfx) {
            screenWidth = gfx->width();
            screenHeight = gfx->height();
        }
    }
    
    ~CityScreensaver() {
        if (backBuffer) {
            delete backBuffer;
        }
    }
    
    void begin(bool isTest = false) {
        enabled = false;
        testMode = isTest;
        activeCities.clear();
        lastSpawnTime = 0;
        needFullRedraw = true;
        randomSeed(esp_random());
        loadCustomCities();
        
        if (gfx && !backBuffer) {
            backBuffer = new Arduino_Canvas(screenWidth, screenHeight, gfx);
            if (backBuffer) {
                if (!backBuffer->begin(GFX_SKIP_OUTPUT_BEGIN)) {
                    delete backBuffer;
                    backBuffer = nullptr;
                    return;
                }
                backBuffer->fillScreen(RGB565_BLACK);
                backBuffer->setFont(&STATION_FONT_NORMAL);
            }
        }
    }
    
    void enable() {
        enabled = true;
        activeCities.clear();
        lastSpawnTime = millis();
        needFullRedraw = true;
        
        if (backBuffer) {
            backBuffer->fillScreen(RGB565_BLACK);
        }
        
        if (gfx) {
            gfx->fillScreen(RGB565_BLACK);
            gfx->setFont(&STATION_FONT_NORMAL);
        }
        
        if (testMode) {
            for (int i = 0; i < 2; i++) {
                std::string cityName = getRandomCity();
                int textWidth, textHeight;
                getTextDimensions(cityName, textWidth, textHeight);
                
                ActiveCity newCity;
                newCity.name = cityName;
                newCity.width = textWidth;
                newCity.height = textHeight;
                getRandomPosition(newCity.x, newCity.y, textWidth, textHeight);
                newCity.alpha = 0;
                newCity.fadingIn = true;
                newCity.holding = false;
                newCity.lastUpdate = millis() - (i * 300);
                newCity.holdStartTime = 0;
                newCity.lastColor = 0;
                activeCities.push_back(newCity);
            }
        }
    }
    
    void disable() {
        enabled = false;
        activeCities.clear();
        if (gfx && !testMode) {
            gfx->fillScreen(RGB565_BLACK);
        }
    }
    
    bool isEnabled() const {
        return enabled;
    }
    
    void update() {
        if (!enabled || !backBuffer) return;
        
        unsigned long now = millis();
        bool changed = false;
        
        int spawnDelay = testMode ? 8000 : 8000;
        int maxCities = testMode ? 3 : 2;
        
        if (activeCities.size() < maxCities && (now - lastSpawnTime) > spawnDelay) {
            std::string cityName = getRandomCity();
            int textWidth, textHeight;
            getTextDimensions(cityName, textWidth, textHeight);
            
            ActiveCity newCity;
            newCity.name = cityName;
            newCity.width = textWidth;
            newCity.height = textHeight;
            getRandomPosition(newCity.x, newCity.y, textWidth, textHeight);
            newCity.alpha = 0;
            newCity.fadingIn = true;
            newCity.holding = false;
            newCity.lastUpdate = now;
            newCity.holdStartTime = 0;
            newCity.lastColor = 0;
            activeCities.push_back(newCity);
            lastSpawnTime = now;
            changed = true;
        }
        
        for (auto& city : activeCities) {
            unsigned long elapsed = now - city.lastUpdate;
            int updateInterval = 40;
            
            if (elapsed >= updateInterval) {
                city.lastUpdate = now;
                
                if (city.fadingIn) {
                    if (city.alpha <= 235) {
                        city.alpha += 5;
                    } else {
                        city.alpha = 255;
                        city.fadingIn = false;
                        city.holding = true;
                        city.holdStartTime = now;  // запоминаем время начала удержания
                    }
                } 
                else if (city.holding) {
                    // Держим максимальную яркость 2 секунды
                    if (now - city.holdStartTime >= 2000) {
                        city.holding = false;
                    }
                }
                else { // fading out (затухание)
                    if (city.alpha >= 10) {
                        city.alpha -= 4;
                    } else {
                        city.alpha = 0;
                    }
                }
                
                changed = true;
            }
        }
        
        // Удаляем полностью затухшие города
        auto it = activeCities.begin();
        while (it != activeCities.end()) {
            if (!it->fadingIn && !it->holding && it->alpha == 0) {
                if (backBuffer) {
                    backBuffer->fillRect(it->x, it->y - 5, 
                                        it->width + 10, it->height + 10, 
                                        RGB565_BLACK);
                }
                it = activeCities.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        
        if (changed) {
            drawChanges();
        }
    }
    
    void drawChanges() {
        if (!enabled || !backBuffer) return;
        
        for (const auto& city : activeCities) {
            uint16_t newColor = getColorForAlpha(city.alpha);
            
            if (newColor != city.lastColor) {
                // Стираем старый текст с небольшим запасом
                backBuffer->fillRect(city.x - 5, city.y - 5, 
                                    city.width + 10, city.height + 10, 
                                    RGB565_BLACK);
                
                // Рисуем новый
                backBuffer->setTextColor(newColor);
                backBuffer->setCursor(city.x, city.y);
                printDisplayText(backBuffer, city.name.c_str());
                
                const_cast<ActiveCity&>(city).lastColor = newColor;
            }
        }
        
        backBuffer->flush();
    }
    
    void draw() {
        if (!enabled || !backBuffer) return;
        
        backBuffer->fillScreen(RGB565_BLACK);
        
        for (const auto& city : activeCities) {
            uint16_t color = getColorForAlpha(city.alpha);
            backBuffer->setTextColor(color);
            backBuffer->setCursor(city.x, city.y);
            printDisplayText(backBuffer, city.name.c_str());
            const_cast<ActiveCity&>(city).lastColor = color;
        }
        
        backBuffer->flush();
        needFullRedraw = false;
    }
    
    // Принудительно очистить экран и перерисовать (вызывать при необходимости)
    void forceRedraw() {
        if (!enabled || !backBuffer) return;
        draw();
    }
};

#endif // SCREENSAVER_H
