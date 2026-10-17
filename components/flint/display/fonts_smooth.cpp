#include "display/fonts_u8g2.h"

#include "display/fonts/flint_time_large.inc"
#include "display/fonts/flint_time_compact.inc"
#include "display/fonts/flint_body.inc"

// VLW data and its reader must outlive the font. Subsets stay in flash;
// LovyanGFX allocates only the small glyph-metric tables at first use.
template <const uint8_t* Data, size_t Size>
inline const lgfx::IFont* flintSmoothFont() {
  struct Font {
    lgfx::PointerWrapper reader{Data, Size};
    lgfx::VLWfont font;
    bool ready;
    Font() : ready(font.loadFont(&reader)) {}
  };
  static Font storage;
  if (storage.ready) return &storage.font;
  return &lgfx::fonts::Font2;
}

const lgfx::IFont* flintFontTimeLarge() {
  return flintSmoothFont<flint_time_large, sizeof(flint_time_large)>();
}

const lgfx::IFont* flintFontTimeCompact() {
  return flintSmoothFont<flint_time_compact, sizeof(flint_time_compact)>();
}

const lgfx::IFont* flintFontBody() {
  return flintSmoothFont<flint_body, sizeof(flint_body)>();
}
