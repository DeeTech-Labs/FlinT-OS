#include "sdk/resources.h"
#include "assets/asset_store.h"
#include "display/fonts_u8g2.h"

const lgfx::IFont* flintFont(FlintFontId id) {
  switch (id) {
    case FlintFontId::TimeLarge:
      return flintFontTimeLarge();
    case FlintFontId::TimeCompact:
      return flintFontTimeCompact();
    case FlintFontId::Body:
    default:
      return flintFontBody();
  }
}

bool flintIconAvailable(FlintIconId id, FlintIconSize size) {
  return assetIconPresent(id, static_cast<uint8_t>(size));
}

void flintDrawIconSized(lgfx::LGFXBase& gfx, FlintIconId id, FlintIconSize size,
                        int cx, int cy, uint16_t fg, uint16_t bg) {
  const uint8_t px = static_cast<uint8_t>(size);
  // Transparent L1/P* blit — do not fill a solid size×size plate (visible frame).
  (void)assetBlitIcon(gfx, id, px, cx, cy, fg, bg);
}

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg) {
  FlintIconSize size = FlintIconSize::Glance;
  if (id == FlintIconId::BrandMark) {
    size = FlintIconSize::Brand;
  }
  flintDrawIconSized(gfx, id, size, cx, cy, fg, bg);
}
