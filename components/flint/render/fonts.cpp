#include "renderer.h"
namespace FlintRender {
#include "fonts/flint_body.inc"
#include "fonts/flint_brand.inc"
#include "fonts/flint_time_compact.inc"
#include "fonts/flint_time_large.inc"
uint32_t fontPixelOffset(unsigned font, unsigned index) {
  const auto *offsets = font == Brand ? flint_brand_offsets
      : font == TimeLarge ? flint_time_large_offsets
      : font == TimeCompact ? flint_time_compact_offsets : flint_body_offsets;
  return offsets[index];
}
const uint8_t *fontData(unsigned font) {
  if (font == Brand)
    return flint_brand;
  return font == TimeLarge     ? flint_time_large
         : font == TimeCompact ? flint_time_compact
                               : flint_body;
}
} // namespace FlintRender
