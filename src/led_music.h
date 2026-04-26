// led_music.h
#ifndef LED_MUSIC_H
#define LED_MUSIC_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#ifndef WS2812B_PIN
#define WS2812B_PIN 48
#endif

#ifndef WS2812B_LED_COUNT
#define WS2812B_LED_COUNT 1
#endif

#ifndef WS2812B_BRIGHTNESS
#define WS2812B_BRIGHTNESS 50
#endif

#ifndef LED_UPDATE_INTERVAL
#define LED_UPDATE_INTERVAL 30
#endif

#ifndef WS2812B_MAX_LED_COUNT
#define WS2812B_MAX_LED_COUNT 300
#endif

class LedMusic {
public:
    struct EffectDefinition {
        const char* key;
        const char* title;
        void (LedMusic::*render)(uint8_t leftVU, uint8_t rightVU, bool isPlaying, bool isPaused, unsigned long now);
    };

private:
    Adafruit_NeoPixel* pixels;
    unsigned long lastUpdate;
    bool initialized;
    uint16_t ledCount;
    uint8_t effectIndex;
    uint16_t frameStep;
    uint8_t fireHeat[WS2812B_MAX_LED_COUNT / 2];
    float stereoPeakLeft;
    float stereoPeakRight;
    float analogPeakLeft;
    float analogPeakRight;

    static constexpr uint8_t EFFECT_COUNT = 11;

    static const EffectDefinition* effectTable();

    uint8_t vuToLevel(uint8_t leftVU, uint8_t rightVU) const {
        float intensity = max(leftVU, rightVU) / 255.0f;
        intensity = pow(intensity, 0.7f);
        return constrain(static_cast<int>(intensity * 255.0f), 0, 255);
    }

    uint32_t wheel(uint8_t pos) const {
        pos = 255 - pos;
        if (pos < 85) {
            return pixels->Color(255 - pos * 3, 0, pos * 3);
        }
        if (pos < 170) {
            pos -= 85;
            return pixels->Color(0, pos * 3, 255 - pos * 3);
        }
        pos -= 170;
        return pixels->Color(pos * 3, 255 - pos * 3, 0);
    }

    uint32_t scaleColor(uint32_t color, uint8_t scale) const {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        r = (static_cast<uint16_t>(r) * scale) / 255;
        g = (static_cast<uint16_t>(g) * scale) / 255;
        b = (static_cast<uint16_t>(b) * scale) / 255;
        return pixels->Color(r, g, b);
    }

    uint32_t blendColor(uint32_t from, uint32_t to, uint8_t amount) const {
        uint8_t fr = (from >> 16) & 0xFF;
        uint8_t fg = (from >> 8) & 0xFF;
        uint8_t fb = from & 0xFF;
        uint8_t tr = (to >> 16) & 0xFF;
        uint8_t tg = (to >> 8) & 0xFF;
        uint8_t tb = to & 0xFF;
        uint8_t r = static_cast<uint8_t>(fr + ((static_cast<int>(tr) - fr) * amount) / 255);
        uint8_t g = static_cast<uint8_t>(fg + ((static_cast<int>(tg) - fg) * amount) / 255);
        uint8_t b = static_cast<uint8_t>(fb + ((static_cast<int>(tb) - fb) * amount) / 255);
        return pixels->Color(r, g, b);
    }

    uint32_t vuGradientColor(uint16_t index, uint16_t total, uint8_t brightness) const {
        if (total <= 1) {
            return scaleColor(pixels->Color(255, 0, 0), brightness);
        }

        uint8_t zone = map(index, 0, total - 1, 0, 255);
        uint32_t base = blendColor(
            blendColor(pixels->Color(0, 220, 180), pixels->Color(0, 220, 0), min<uint8_t>(zone, 85) * 3),
            blendColor(pixels->Color(220, 220, 0), pixels->Color(255, 0, 0), max(0, static_cast<int>(zone) - 170) * 3),
            zone
        );
        return scaleColor(base, brightness);
    }

    uint32_t heatColor(uint8_t heat) const {
        if (heat < 85) {
            return pixels->Color(heat * 3, 0, 0);
        }
        if (heat < 170) {
            return pixels->Color(255, (heat - 85) * 2, 0);
        }
        return pixels->Color(255, min(255, 170 + (heat - 170)), 0);
    }

    void resetDynamicState() {
        frameStep = 0;
        stereoPeakLeft = 0.0f;
        stereoPeakRight = 0.0f;
        analogPeakLeft = 0.0f;
        analogPeakRight = 0.0f;
        memset(fireHeat, 0, sizeof(fireHeat));
    }

    void fillAll(uint32_t color) {
        for (uint16_t i = 0; i < ledCount; ++i) {
            pixels->setPixelColor(i, color);
        }
    }

    bool createStrip(uint16_t count) {
        if (pixels) {
            pixels->clear();
            pixels->show();
            delete pixels;
            pixels = nullptr;
        }

        ledCount = constrain(count, 1, WS2812B_MAX_LED_COUNT);
        pixels = new Adafruit_NeoPixel(ledCount, WS2812B_PIN, NEO_GRB + NEO_KHZ800);
        if (!pixels) {
            initialized = false;
            return false;
        }

        pixels->begin();
        pixels->setBrightness(WS2812B_BRIGHTNESS);
        pixels->clear();
        pixels->show();
        initialized = true;
        lastUpdate = 0;
        resetDynamicState();
        return true;
    }

    void renderRunningRainbow(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        uint8_t level = static_cast<uint8_t>(max(40, static_cast<int>(vuToLevel(leftVU, rightVU))));
        for (uint16_t i = 0; i < ledCount; ++i) {
            uint8_t hue = static_cast<uint8_t>((i * 256 / max(1, static_cast<int>(ledCount)) + frameStep * 4) & 0xFF);
            pixels->setPixelColor(i, scaleColor(wheel(hue), level));
        }
    }

    void renderRunningFire(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long now) {
        uint8_t level = static_cast<uint8_t>(max(55, static_cast<int>(vuToLevel(leftVU, rightVU))));
        for (uint16_t i = 0; i < ledCount; ++i) {
            uint8_t flicker = static_cast<uint8_t>((sin((now / 45.0f) + i * 0.65f) * 0.5f + 0.5f) * level);
            uint8_t red = constrain(level + flicker / 3, 0, 255);
            uint8_t green = flicker / 2;
            uint8_t blue = flicker / 18;
            pixels->setPixelColor(i, pixels->Color(red, green, blue));
        }
    }

    void renderPolice(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long now) {
        uint8_t level = static_cast<uint8_t>(max(80, static_cast<int>(vuToLevel(leftVU, rightVU))));
        bool firstHalfBlue = ((now / 180) % 2) == 0;
        for (uint16_t i = 0; i < ledCount; ++i) {
            bool firstHalf = i < (ledCount / 2);
            bool blueSide = firstHalf ? firstHalfBlue : !firstHalfBlue;
            if (blueSide) {
                pixels->setPixelColor(i, pixels->Color(0, 0, level));
            } else {
                pixels->setPixelColor(i, pixels->Color(level, 0, 0));
            }
        }
    }

    void renderGlobalSmoothColor(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        uint8_t level = static_cast<uint8_t>(max(45, static_cast<int>(vuToLevel(leftVU, rightVU))));
        uint8_t hue = static_cast<uint8_t>((frameStep * 3) & 0xFF);
        fillAll(scaleColor(wheel(hue), level));
    }

    void renderVuMeter(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        uint8_t level = max(leftVU, rightVU);
        uint16_t lit = map(level, 0, 255, 0, ledCount);
        for (uint16_t i = 0; i < ledCount; ++i) {
            if (i < lit) {
                uint8_t zone = map(i, 0, max(1, static_cast<int>(ledCount - 1)), 0, 255);
                pixels->setPixelColor(i, blendColor(pixels->Color(0, 180, 0), pixels->Color(220, 0, 0), zone));
            } else {
                pixels->setPixelColor(i, pixels->Color(0, 0, 0));
            }
        }
    }

    void renderComet(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        uint8_t level = static_cast<uint8_t>(max(50, static_cast<int>(vuToLevel(leftVU, rightVU))));
        int head = frameStep % max(1, static_cast<int>(ledCount));
        for (uint16_t i = 0; i < ledCount; ++i) {
            int dist = abs(static_cast<int>(i) - head);
            dist = min(dist, abs(static_cast<int>(ledCount) - dist));
            uint8_t tail = dist > 6 ? 0 : static_cast<uint8_t>(level * (6 - dist) / 6);
            pixels->setPixelColor(i, pixels->Color(tail / 6, tail / 2, tail));
        }
    }

    void renderAurora(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long now) {
        uint8_t level = static_cast<uint8_t>(max(50, static_cast<int>(vuToLevel(leftVU, rightVU))));
        for (uint16_t i = 0; i < ledCount; ++i) {
            float wave1 = sin((now / 260.0f) + i * 0.32f);
            float wave2 = sin((now / 410.0f) - i * 0.21f);
            uint8_t mix = static_cast<uint8_t>((wave1 * 0.5f + wave2 * 0.5f + 1.0f) * 127.5f);
            uint32_t color = blendColor(pixels->Color(0, level / 2, level), pixels->Color(0, level, level / 3), mix);
            pixels->setPixelColor(i, color);
        }
    }

    void renderStereoVuMirror(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        const uint16_t leftCount = ledCount / 2;
        const uint16_t rightCount = ledCount - leftCount;
        if (leftCount == 0 || rightCount == 0) {
            renderVuMeter(leftVU, rightVU, true, false, 0);
            return;
        }

        const float peakDecay = 0.32f;
        const uint16_t litLeft = map(leftVU, 0, 255, 0, leftCount);
        const uint16_t litRight = map(rightVU, 0, 255, 0, rightCount);

        stereoPeakLeft = max(stereoPeakLeft - peakDecay, static_cast<float>(litLeft));
        stereoPeakRight = max(stereoPeakRight - peakDecay, static_cast<float>(litRight));

        fillAll(0);

        for (uint16_t i = 0; i < leftCount; ++i) {
            if (i < litLeft) {
                uint8_t brightness = static_cast<uint8_t>(160 + (i * 95) / max<uint16_t>(1, leftCount - 1));
                pixels->setPixelColor(leftCount - 1 - i, vuGradientColor(i, leftCount, brightness));
            } else if (i == litLeft && litLeft > 0) {
                pixels->setPixelColor(leftCount - 1 - i, vuGradientColor(i, leftCount, 55));
            }
        }

        for (uint16_t i = 0; i < rightCount; ++i) {
            if (i < litRight) {
                uint8_t brightness = static_cast<uint8_t>(160 + (i * 95) / max<uint16_t>(1, rightCount - 1));
                pixels->setPixelColor(leftCount + i, vuGradientColor(i, rightCount, brightness));
            } else if (i == litRight && litRight > 0) {
                pixels->setPixelColor(leftCount + i, vuGradientColor(i, rightCount, 55));
            }
        }

        if (stereoPeakLeft > 0.0f) {
            uint16_t peak = min<uint16_t>(leftCount, static_cast<uint16_t>(stereoPeakLeft));
            if (peak > 0) {
                uint16_t idx = leftCount - peak;
                pixels->setPixelColor(idx, pixels->Color(255, 255, 255));
                if (idx + 1 < leftCount) pixels->setPixelColor(idx + 1, pixels->Color(60, 60, 60));
            }
        }

        if (stereoPeakRight > 0.0f) {
            uint16_t peak = min<uint16_t>(rightCount, static_cast<uint16_t>(stereoPeakRight));
            if (peak > 0) {
                uint16_t idx = leftCount + peak - 1;
                pixels->setPixelColor(idx, pixels->Color(255, 255, 255));
                if (idx > leftCount) pixels->setPixelColor(idx - 1, pixels->Color(60, 60, 60));
            }
        }
    }

    void renderRainbowFlowMirror(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        const uint16_t leftCount = ledCount / 2;
        const uint16_t rightCount = ledCount - leftCount;
        const uint16_t halfSpan = min(leftCount, rightCount);
        if (halfSpan == 0) {
            renderRunningRainbow(leftVU, rightVU, true, false, 0);
            return;
        }

        const uint8_t vuAvg = static_cast<uint8_t>((static_cast<uint16_t>(leftVU) + rightVU) / 2);
        const bool hasSignal = vuAvg > 8;
        uint16_t litSpan = halfSpan;

        if (hasSignal) {
            uint16_t litMin = min<uint16_t>(halfSpan, 3);
            litSpan = litMin + map(vuAvg, 0, 255, 0, halfSpan - litMin);
            if (vuAvg > 180) {
                frameStep += map(vuAvg, 181, 255, 10, 48);
            }
        } else {
            litSpan = halfSpan;
        }

        fillAll(0);

        for (uint16_t i = 0; i < litSpan; ++i) {
            uint8_t hue = static_cast<uint8_t>((frameStep * (hasSignal ? 3 : 1) + (i * 256 / max<uint16_t>(1, litSpan))) & 0xFF);
            uint32_t color = scaleColor(wheel(hue), hasSignal ? 220 : 160);
            pixels->setPixelColor(leftCount - 1 - i, color);
            pixels->setPixelColor(leftCount + i, color);
        }

        if (leftCount != rightCount && ledCount > 0) {
            uint16_t edgeIndex = ledCount - 1;
            if (edgeIndex >= leftCount + litSpan) {
                pixels->setPixelColor(edgeIndex, hasSignal ? scaleColor(wheel(frameStep & 0xFF), 100) : 0);
            }
        }
    }

    void renderHeatFireMirror(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        const uint16_t leftCount = ledCount / 2;
        const uint16_t rightCount = ledCount - leftCount;
        const uint16_t halfSpan = min(leftCount, rightCount);
        if (halfSpan == 0) {
            renderRunningFire(leftVU, rightVU, true, false, millis());
            return;
        }

        const uint8_t vuAvg = static_cast<uint8_t>((static_cast<uint16_t>(leftVU) + rightVU) / 2);
        const uint8_t cooling = static_cast<uint8_t>(20 - (static_cast<uint32_t>(vuAvg) * 14U / 255U));

        for (uint16_t i = 0; i < halfSpan; ++i) {
            uint8_t cool = static_cast<uint8_t>(random(max<int>(1, cooling / 2), cooling + 1));
            fireHeat[i] = fireHeat[i] > cool ? fireHeat[i] - cool : 0;
        }

        if (halfSpan >= 3) {
            for (int i = static_cast<int>(halfSpan) - 1; i >= 2; --i) {
                fireHeat[i] = static_cast<uint8_t>((fireHeat[i - 1] + fireHeat[i - 2] + fireHeat[i - 2]) / 3);
            }
        }

        uint8_t sparkRange = static_cast<uint8_t>(min<uint16_t>(halfSpan, 3));
        if (sparkRange > 0) {
            uint8_t sparkMin = static_cast<uint8_t>(150 + (static_cast<uint32_t>(vuAvg) * 60U / 255U));
            uint8_t sparkMax = static_cast<uint8_t>(200 + (static_cast<uint32_t>(vuAvg) * 55U / 255U));
            uint8_t pos = sparkRange == 1 ? 0 : static_cast<uint8_t>(random(0, sparkRange));
            fireHeat[pos] = min(255, fireHeat[pos] + static_cast<int>(random(sparkMin, sparkMax + 1)));

            if (vuAvg > 180) {
                uint8_t pos2 = sparkRange == 1 ? 0 : static_cast<uint8_t>(random(0, sparkRange));
                fireHeat[pos2] = min(255, fireHeat[pos2] + static_cast<int>(random(180, 256)));
            }
        }

        fillAll(0);
        for (uint16_t i = 0; i < halfSpan; ++i) {
            uint32_t color = heatColor(fireHeat[i]);
            pixels->setPixelColor(leftCount - 1 - i, color);
            pixels->setPixelColor(leftCount + i, color);
        }

        if (leftCount != rightCount) {
            pixels->setPixelColor(ledCount - 1, heatColor(fireHeat[0] / 2));
        }
    }

    void renderAnalogMeter(uint8_t leftVU, uint8_t rightVU, bool, bool, unsigned long) {
        const uint16_t leftCount = ledCount / 2;
        const uint16_t rightCount = ledCount - leftCount;
        if (leftCount == 0 || rightCount == 0) {
            renderVuMeter(leftVU, rightVU, true, false, 0);
            return;
        }

        const float decay = max(0.08f, min(leftCount, rightCount) / 27.0f);
        const uint16_t litLeft = map(leftVU, 0, 255, 0, leftCount);
        const uint16_t litRight = map(rightVU, 0, 255, 0, rightCount);

        analogPeakLeft = max(analogPeakLeft - decay, static_cast<float>(litLeft));
        analogPeakRight = max(analogPeakRight - decay, static_cast<float>(litRight));

        fillAll(0);

        for (uint16_t i = 0; i < leftCount; ++i) {
            if (i < litLeft) {
                pixels->setPixelColor(i, pixels->Color(220, 220, 220));
            }
        }

        for (uint16_t i = 0; i < rightCount; ++i) {
            if (i < litRight) {
                pixels->setPixelColor(leftCount + i, pixels->Color(220, 220, 220));
            }
        }

        if (analogPeakLeft > 0.0f) {
            uint16_t idx = min<uint16_t>(leftCount - 1, static_cast<uint16_t>(analogPeakLeft));
            pixels->setPixelColor(idx, pixels->Color(255, 0, 0));
            if (idx + 1 < leftCount) pixels->setPixelColor(idx + 1, pixels->Color(180, 0, 0));
        }

        if (analogPeakRight > 0.0f) {
            uint16_t idx = leftCount + min<uint16_t>(rightCount - 1, static_cast<uint16_t>(analogPeakRight));
            pixels->setPixelColor(idx, pixels->Color(255, 0, 0));
            if (idx + 1 < ledCount) pixels->setPixelColor(idx + 1, pixels->Color(180, 0, 0));
        }
    }

public:
    LedMusic()
        : pixels(nullptr), lastUpdate(0), initialized(false), ledCount(WS2812B_LED_COUNT), effectIndex(0),
          frameStep(0), fireHeat{0}, stereoPeakLeft(0.0f), stereoPeakRight(0.0f),
          analogPeakLeft(0.0f), analogPeakRight(0.0f) {}

    ~LedMusic() {
        if (pixels) {
            pixels->clear();
            pixels->show();
            delete pixels;
        }
    }

    bool begin(uint16_t count = WS2812B_LED_COUNT) {
        bool ok = createStrip(count);
        if (ok) {
            Serial.println("LED Music initialized on pin " + String(WS2812B_PIN) + ", count=" + String(ledCount));
        }
        return ok;
    }

    bool setLedCount(uint16_t count) {
        return createStrip(count);
    }

    uint16_t getLedCount() const { return ledCount; }

    uint8_t getEffectCount() const { return EFFECT_COUNT; }

    uint8_t getEffectIndex() const { return effectIndex; }

    const char* getEffectName() const { return effectTable()[effectIndex].title; }

    const char* getEffectKey() const { return effectTable()[effectIndex].key; }

    const EffectDefinition& getEffectDefinition(uint8_t index) const {
        return effectTable()[constrain(index, 0, EFFECT_COUNT - 1)];
    }

    void setEffectIndex(int index) {
        effectIndex = constrain(index, 0, EFFECT_COUNT - 1);
        resetDynamicState();
    }

    void nextEffect() {
        effectIndex = (effectIndex + 1) % EFFECT_COUNT;
        resetDynamicState();
    }

    void prevEffect() {
        effectIndex = (effectIndex == 0) ? (EFFECT_COUNT - 1) : (effectIndex - 1);
        resetDynamicState();
    }

    void update(int vuLevel, bool isPlaying, bool isPaused) {
        if (!initialized || !pixels) return;

        unsigned long now = millis();
        if (now - lastUpdate < LED_UPDATE_INTERVAL) return;
        lastUpdate = now;

        if (!isPlaying || isPaused) {
            off();
            return;
        }

        uint8_t leftVU = (vuLevel >> 8) & 0xFF;
        uint8_t rightVU = vuLevel & 0xFF;
        frameStep++;

        auto render = effectTable()[effectIndex].render;
        (this->*render)(leftVU, rightVU, isPlaying, isPaused, now);
        pixels->show();
    }

    void off() {
        if (!initialized || !pixels) return;
        pixels->clear();
        pixels->show();
    }
};

inline const LedMusic::EffectDefinition* LedMusic::effectTable() {
    static const EffectDefinition effects[EFFECT_COUNT] = {
        {"rainbow_run", "Running rainbow", &LedMusic::renderRunningRainbow},
        {"running_fire", "Running fire", &LedMusic::renderRunningFire},
        {"police", "Police", &LedMusic::renderPolice},
        {"global_fade", "Smooth global color", &LedMusic::renderGlobalSmoothColor},
        {"vu_meter", "VU meter", &LedMusic::renderVuMeter},
        {"blue_comet", "Blue comet", &LedMusic::renderComet},
        {"aurora", "Aurora", &LedMusic::renderAurora},
        {"stereo_vu", "Stereo VU mirror", &LedMusic::renderStereoVuMirror},
        {"rainbow_flow", "Rainbow flow mirror", &LedMusic::renderRainbowFlowMirror},
        {"heat_fire", "Heat fire mirror", &LedMusic::renderHeatFireMirror},
        {"analog_meter", "Analog meter", &LedMusic::renderAnalogMeter},
    };
    return effects;
}

#endif // LED_MUSIC_H
