#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include <Arduino_GFX_Library.h>
#include <vector>
#include <string>
#include <cmath>

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
        unsigned long lastUpdate;
        uint16_t lastColor;
        int width;  // Ширина текста в пикселях
        int height; // Высота текста
    };
    
    std::vector<ActiveCity> activeCities;
    unsigned long lastSpawnTime;
    bool enabled;
    bool testMode;
    bool needFullRedraw;
    
    int randomRange(int min, int max) {
        return min + random(max - min + 1);
    }
    
    std::string getRandomCity() {
        return UKRAINE_CITIES[random(0, UKRAINE_CITIES.size())];
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
        
        while (!found && attempt < maxAttempts) {
            x = randomRange(20, screenWidth - textWidth - 20);
            y = randomRange(20, screenHeight - textHeight - 20);
            
            if (!checkCollision(x, y, textWidth, textHeight)) {
                found = true;
            }
            attempt++;
        }
        
        // Если не нашли свободное место, просто ставим в случайное место
        if (!found) {
            x = randomRange(20, screenWidth - textWidth - 20);
            y = randomRange(20, screenHeight - textHeight - 20);
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
        
        if (gfx && !backBuffer) {
            backBuffer = new Arduino_Canvas(screenWidth, screenHeight, gfx);
            if (backBuffer) {
                backBuffer->begin();
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
                newCity.lastUpdate = millis() - (i * 300);
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
        
        int spawnDelay = testMode ? 1500 : 2500;
        int maxCities = testMode ? 6 : 5;
        
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
            newCity.lastUpdate = now;
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
                        city.alpha += 10;
                    } else {
                        city.alpha = 255;
                        city.fadingIn = false;
                    }
                } else {
                    if (city.alpha >= 10) {
                        city.alpha -= 8;
                    } else {
                        city.alpha = 0;
                    }
                }
                
                changed = true;
            }
        }
        
        auto it = activeCities.begin();
        while (it != activeCities.end()) {
            if (!it->fadingIn && it->alpha == 0) {
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