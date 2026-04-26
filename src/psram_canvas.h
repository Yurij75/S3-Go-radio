#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

#include <new>

class PsramCanvas : public Arduino_Canvas {
public:
  PsramCanvas(int16_t w, int16_t h, Arduino_G* output, bool preferPsram,
              int16_t output_x = 0, int16_t output_y = 0, uint8_t rotation = 0)
      : Arduino_Canvas(w, h, output, output_x, output_y, rotation),
        _preferPsram(preferPsram) {}

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    if ((speed != GFX_SKIP_OUTPUT_BEGIN) && _output) {
      if (!_output->begin(speed)) {
        return false;
      }
    }

    if (_framebuffer) {
      return true;
    }

    const size_t size = static_cast<size_t>(_width) * static_cast<size_t>(_height) * 2U;

    if (_preferPsram && psramFound()) {
      _framebuffer = static_cast<uint16_t*>(
          heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }

    if (!_framebuffer) {
#if defined(ESP32)
      _framebuffer = static_cast<uint16_t*>(aligned_alloc(16, size));
#else
      _framebuffer = static_cast<uint16_t*>(malloc(size));
#endif
    }

    return _framebuffer != nullptr;
  }

private:
  bool _preferPsram;
};

inline Arduino_Canvas* createManagedCanvas(int16_t w, int16_t h, Arduino_GFX* output,
                                           bool preferPsram, const char* name) {
  auto* canvas = new (std::nothrow) PsramCanvas(w, h, output, preferPsram);
  if (!canvas) {
    Serial.printf("Canvas alloc failed: %s object\r\n", name);
    return nullptr;
  }

  if (!canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    Serial.printf("Canvas begin failed: %s (%dx%d, %u bytes)\r\n",
                  name, w, h, static_cast<unsigned>(w * h * 2U));
    delete canvas;
    return nullptr;
  }

  return canvas;
}
