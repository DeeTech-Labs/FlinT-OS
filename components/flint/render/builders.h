#pragma once
#include "renderer.h"
#include <stdio.h>

namespace FlintRender {
constexpr uint16_t Background = 0x0863, Surface = 0x10a5, Accent = 0x3bdf,
                   Muted = 0x8d16, White = 0xef7d;
inline Scene begin(const char *title) {
  Scene s;
  rect(s, 0, 0, 280, 240, Background);
  rect(s, 14, 12, 5, 19, Accent, 2);
  text(s, 29, 29, title, White);
  rect(s, 14, 42, 252, 1, 0x19a8);
  return s;
}
inline void fit(char *out, size_t capacity, const char *value, int width) {
  if (!capacity)
    return;
  size_t n = strlen(value);
  if (n >= capacity)
    n = capacity - 1;
  while (n && (uint8_t(value[n]) & 0xc0) == 0x80)
    --n;
  memcpy(out, value, n);
  out[n] = 0;
  if (textWidth(Body, reinterpret_cast<const uint8_t *>(out), n) <= width)
    return;
  while (n) {
    do {
      --n;
    } while (n && (uint8_t(out[n]) & 0xc0) == 0x80);
    out[n] = 0;
    if (textWidth(Body, reinterpret_cast<const uint8_t *>(out), n) + 20 <=
        width)
      break;
  }
  if (n + 3 < capacity) {
    memcpy(out + n, "…", 4);
  }
}
inline void row(Scene &s, int index, const char *label, const char *value,
                bool selected) {
  const int y = 51 + index * 40;
  if (selected) {
    rect(s, 10, y, 260, 36, 0x194e, 7);
    rect(s, 10, y + 8, 3, 20, Accent, 1);
  }
  char left[96], right[96];
  const bool hasValue = value && *value;
  int valueWidth = 0;
  if (hasValue) {
    fit(right, sizeof(right), value, 118);
    valueWidth = textWidth(Body, reinterpret_cast<const uint8_t *>(right),
                           strlen(right));
  }
  fit(left, sizeof(left), label, hasValue ? 226 - valueWidth : 238);
  text(s, 21, y + 25, left, selected ? White : Muted);
  if (hasValue)
    text(s, 259 - valueWidth, y + 25, right, White);
}
inline void footer(Scene &s, const char *hint) {
  text(s, 140, 232, hint, Muted, Body, true);
}
// Both the editor and firmware supply resolved text and reuse the exact layer
// geometry.
inline Scene studio(uint32_t background, uint32_t foreground,
                    const uint8_t *icon, int iconX, int iconY, int scale,
                    bool showIcon, unsigned order, const char *time,
                    Font timeFont, int timeY, const char *date, int dateY,
                    const char *meridiem = "") {
  Scene scene;
  rect(scene, 0, 0, 280, 240, rgb24(background));
  const auto color = rgb24(foreground);
  static const unsigned orders[6][3]{{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                     {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  for (const auto layer : orders[order < 6 ? order : 0]) {
    if (layer == 0 && showIcon)
      bitmap(scene, iconX, iconY, icon, scale, color);
    if (layer == 1) {
      text(scene, 140, timeY, time, color, timeFont, true);
      if (*meridiem)
        text(scene, 140, timeY + 24, meridiem, color, Body, true);
    }
    if (layer == 2 && *date)
      text(scene, 140, dateY, date, color, Body, true);
  }
  return scene;
}
} // namespace FlintRender
