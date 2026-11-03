#pragma once
#include "builders.h"

namespace FlintRender {
enum class ClockStyle : uint8_t { Digital, Blocks, Quiet };
// Resolved strings keep layout portable: firmware and visual checks share pixels.
struct ClockText {
  const char *time, *hours, *minutes, *seconds, *date, *status, *meridiem;
  bool showSeconds;
};
inline Scene clockScene(ClockStyle style, const ClockText &c, const ThemeColors& theme = currentTheme()) {
  const auto Background=theme.background, Surface=theme.surface, Accent=theme.accent,
             Muted=theme.muted, White=theme.foreground, Selected=theme.selected;
  Scene s;
  rect(s, 0, 0, 280, 240, Background);
  char date[96];
  fit(date, sizeof(date), c.date, 236);
  if (style == ClockStyle::Blocks) {
    text(s, 24, 32, "SPARK", Muted);
    text(s, 256 - textWidth(Body, reinterpret_cast<const uint8_t *>(c.status),
                           strlen(c.status)), 32, c.status, Muted);
    rect(s, 18, 59, 116, 110, Surface, 12);
    rect(s, 146, 59, 116, 110, Selected, 12);
    text(s, 76, 139, c.hours, White, TimeLarge, true);
    text(s, 204, 139, c.minutes, Accent, TimeLarge, true);
    text(s, 140, 196, date, Muted, Body, true);
  } else if (style == ClockStyle::Quiet) {
    text(s, 140, 59, date, Muted, Body, true);
    text(s, 140, 145, c.time, White, TimeCompact, true);
    rect(s, 128, 173, 24, 2, Accent, 1);
  } else {
    text(s, 24, 36, "SPARK", Accent);
    text(s, 256 - textWidth(Body, reinterpret_cast<const uint8_t *>(c.status),
                           strlen(c.status)), 36, c.status, Muted);
    text(s, 140, 138, c.time, White, TimeLarge, true);
    text(s, 140, 179, date, Muted, Body, true);
  }
  // Stable operations isolate second ticks to this small bottom region.
  text(s, 140, 222, c.showSeconds ? c.seconds : "", Accent, Body, true);
  text(s, 233, 222, c.meridiem, Muted);
  return s;
}
} // namespace FlintRender
