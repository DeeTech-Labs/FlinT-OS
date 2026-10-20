#include "renderer.h"
namespace FlintRender {
#include "fonts/flint_body.inc"
#include "fonts/flint_brand.inc"
#include "fonts/flint_time_compact.inc"
#include "fonts/flint_time_large.inc"
const uint8_t *fontData(unsigned font) {
  if (font == Brand)
    return flint_brand;
  return font == TimeLarge     ? flint_time_large
         : font == TimeCompact ? flint_time_compact
                               : flint_body;
}
} // namespace FlintRender
