#include "ui/splash.h"

#include "display/display.h"
#include "sdk/resources.h"
#include "util/flint_sys.h"

static constexpr uint32_t kSplashTotalMs = 3000;
static constexpr uint32_t kSplashAnimMs = 900;
static constexpr uint32_t kSplashFrameMs = 33;  // ~30 fps

static void splashPaint(uint8_t t /*0..255*/) {
  auto& gfx = displayGfx();
  const uint16_t bg = rgb565(8, 10, 18);
  const uint16_t accentFull = rgb565(80, 230, 220);
  const uint16_t mutedFull = rgb565(120, 140, 160);
  const uint16_t accent = colorLerp(bg, accentFull, t);

  // Ease size: mark grows in during the first part of the fade.
  const int grow = 40 + static_cast<int>((16 * t) / 255);  // 40→56
  const int hole = grow - 16;
  const int cx = SCR_W / 2;
  const int cy = 98;

  gfx.fillScreen(bg);
  gfx.fillRoundRect(cx - grow / 2, cy - grow / 2, grow, grow, 10, accent);
  gfx.fillRoundRect(cx - hole / 2, cy - hole / 2, hole, hole, 8, bg);
  gfx.fillCircle(cx, cy, 6 + (4 * t) / 255, accent);

  if (t > 80) {
    const uint8_t tt = static_cast<uint8_t>(((t - 80) * 255) / 175);
    const uint16_t a = colorLerp(bg, accentFull, tt);
    const uint16_t m = colorLerp(bg, mutedFull, tt);
    displayDrawUtf8Centered("FlinT", 160, a, flintFont(FlintFontId::Body));
    displayDrawUtf8Centered("DeeTech Labs", 190, m, flintFont(FlintFontId::Body));
  }

  displayPresent();
}

void splashShow() {
  if (!displayHasCanvas()) {
    // No canvas — still hold brand color on panel.
    tft.fillScreen(rgb565(8, 10, 18));
    flintDelay(kSplashTotalMs);
    return;
  }

  const uint32_t t0 = flintMillis();
  uint32_t lastFrame = 0;

  while (true) {
    const uint32_t elapsed = flintMillis() - t0;
    if (elapsed >= kSplashTotalMs) {
      break;
    }

    uint8_t t = 255;
    if (elapsed < kSplashAnimMs) {
      t = static_cast<uint8_t>((elapsed * 255UL) / kSplashAnimMs);
    }

    if (elapsed == 0 || (elapsed - lastFrame) >= kSplashFrameMs || t == 255) {
      splashPaint(t);
      lastFrame = elapsed;
      // Once fully painted, no need to redraw every frame.
      if (t >= 255) {
        const uint32_t remain = kSplashTotalMs - elapsed;
        if (remain > 0) {
          flintDelay(remain);
        }
        break;
      }
    }

    flintDelay(5);
  }

  // Ensure final frame is solid brand before face takes over.
  splashPaint(255);
}
