#pragma once

#include <Arduino.h>

inline uint32_t decodeUtf8Codepoint(const String& text, size_t& index) {
  const uint8_t first = static_cast<uint8_t>(text[index]);
  if ((first & 0x80) == 0) {
    index += 1;
    return first;
  }

  if ((first & 0xE0) == 0xC0 && index + 1 < text.length()) {
    const uint8_t second = static_cast<uint8_t>(text[index + 1]);
    index += 2;
    return ((first & 0x1F) << 6) | (second & 0x3F);
  }

  if ((first & 0xF0) == 0xE0 && index + 2 < text.length()) {
    const uint8_t second = static_cast<uint8_t>(text[index + 1]);
    const uint8_t third = static_cast<uint8_t>(text[index + 2]);
    index += 3;
    return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
  }

  index += 1;
  return '?';
}

inline int16_t mapUnicodeToDisplayChar(uint32_t codepoint) {
  if (codepoint <= 0x7F) return static_cast<int16_t>(codepoint);

  if (codepoint >= 0x0410 && codepoint <= 0x042F) {
    return static_cast<int16_t>(0x90 + (codepoint - 0x0410));
  }
  if (codepoint >= 0x0430 && codepoint <= 0x043F) {
    return static_cast<int16_t>(0xB0 + (codepoint - 0x0430));
  }
  if (codepoint >= 0x0440 && codepoint <= 0x044F) {
    return static_cast<int16_t>(0x80 + (codepoint - 0x0440));
  }

  switch (codepoint) {
    case 0x0401: return 0xC0; // Ё
    case 0x0451: return 0xC1; // ё
    case 0x0404: return 0xC2; // Є
    case 0x0454: return 0xC3; // є
    case 0x0406: return 0xC4; // І
    case 0x0456: return 0xC5; // і
    case 0x0407: return 0xC6; // Ї
    case 0x0457: return 0xC7; // ї
    case 0x0490: return 0xC8; // Ґ
    case 0x0491: return 0xC9; // ґ
    case 0x2013:
    case 0x2014: return '-';
    case 0x00A0: return ' ';
    default: return '?';
  }
}

inline String encodeDisplayText(const String& text) {
  String out;
  out.reserve(text.length());

  for (size_t i = 0; i < text.length();) {
    const uint32_t codepoint = decodeUtf8Codepoint(text, i);
    const int16_t mapped = mapUnicodeToDisplayChar(codepoint);
    out += static_cast<char>(mapped >= 0 ? mapped : '?');
  }

  return out;
}

template <typename TGfx>
inline void getDisplayTextBounds(TGfx* gfx, const String& text, int16_t x, int16_t y,
                                 int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  const String encoded = encodeDisplayText(text);
  gfx->getTextBounds(encoded.c_str(), x, y, x1, y1, w, h);
}

template <typename TGfx>
inline void printDisplayText(TGfx* gfx, const String& text) {
  const String encoded = encodeDisplayText(text);
  gfx->print(encoded);
}
