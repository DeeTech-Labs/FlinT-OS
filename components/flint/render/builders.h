#pragma once
#include "renderer.h"
#include <stdio.h>

namespace FlintRender {
// Shared watch palette: quiet surfaces, one mint focus color, high contrast text.
inline uint16_t Background = 0x0842, Surface = 0x10c4, Accent = 0x7ef5,
                   Muted = 0x94b2, White = 0xf7be, Selected = 0x1947;
struct ThemeColors {
  uint16_t background = 0x0842, surface = 0x10c4, accent = 0x7ef5,
           muted = 0x94b2, foreground = 0xf7be, selected = 0x1947;
};
inline ThemeColors currentTheme() { return {Background, Surface, Accent, Muted, White, Selected}; }
inline void setTheme(const ThemeColors& theme) {
  Background=theme.background; Surface=theme.surface; Accent=theme.accent;
  Muted=theme.muted; White=theme.foreground; Selected=theme.selected;
}
constexpr int VisibleRows = 3;
inline Scene begin(const char *title) {
  Scene s;
  rect(s, 0, 0, 280, 240, Background);
  text(s, 20, 32, title, White);
  rect(s, 20, 43, 240, 1, Surface);
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
  const int y = 49 + index * 52;
  rect(s, 14, y, 252, 48, selected ? Selected : Background, 9);
  rect(s, 14, y + 16, 3, 16, selected ? Accent : Background, 1);
  char fitted[96];
  const bool hasValue = value && *value;
  fit(fitted, sizeof(fitted), label, 224);
  text(s, 26, y + (hasValue ? 20 : 31), fitted,
       hasValue ? Muted : White);
  if (hasValue) {
    fit(fitted, sizeof(fitted), value, 224);
    text(s, 26, y + 41, fitted, selected ? Accent : White);
  }
}
inline void counter(Scene &s, int current, int total) {
  char value[32];
  snprintf(value, sizeof(value), "%d/%d", current + 1, total);
  text(s, 260 - textWidth(Body, reinterpret_cast<const uint8_t *>(value),
                        strlen(value)), 32, value, Muted);
}
inline void footer(Scene &s, const char *hint) {
  char fitted[96];
  fit(fitted, sizeof(fitted), hint, 232);
  text(s, 140, 224, fitted, Muted, Body, true);
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
