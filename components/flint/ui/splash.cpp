#include "ui/splash.h"

#include "anim/anim.h"
#include "display/display.h"
#include "display/ui_tokens.h"
#include "sdk/resources.h"
#include "util/flint_sys.h"

static DisplayRect splashContentRect(uint8_t t) {
  const int cy = UiTok::splashMarkCy(SCR_H);
  const int maxHalf = UiTok::kSplashMarkMax / 2 + 6;
  int y0 = cy - maxHalf;
  int y1 = cy + maxHalf;
  if (t > 80) {
    // Title + subtitle baselines with font slack.
    const int subBottom = UiTok::splashSubY(SCR_H) + 18;
    if (subBottom > y1) {
      y1 = subBottom;
    }
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y1 > SCR_H) {
    y1 = SCR_H;
  }
  return DisplayRect{0, static_cast<int16_t>(y0), static_cast<int16_t>(SCR_W),
                     static_cast<int16_t>(y1 - y0)};
}

static void splashPaint(uint8_t t /*0..255*/, bool fullPresent) {
  auto& gfx = displayGfx();
  const uint16_t bg = rgb565(8, 12, 20);           // day refined bgTop
  const uint16_t accentFull = rgb565(72, 224, 214); // day accent
  const uint16_t mutedFull = rgb565(132, 158, 178);
  const uint16_t accent = colorLerp(bg, accentFull, t);

  const int grow =
      UiTok::kSplashMarkMin
      + static_cast<int>(((UiTok::kSplashMarkMax - UiTok::kSplashMarkMin) * t) / 255);
  const int hole = grow - 16;
  const int cx = SCR_W / 2;
  const int cy = UiTok::splashMarkCy(SCR_H);
  const DisplayRect band = splashContentRect(t);

  if (fullPresent) {
    gfx.fillScreen(bg);
  } else {
    gfx.fillRect(band.x, band.y, band.w, band.h, bg);
  }

  gfx.fillRoundRect(cx - grow / 2, cy - grow / 2, grow, grow, UiTok::kRadiusMd, accent);
  gfx.fillRoundRect(cx - hole / 2, cy - hole / 2, hole, hole, UiTok::kRadiusSm, bg);
  gfx.fillCircle(cx, cy, 6 + (4 * t) / 255, accent);

  if (t > 80) {
    const uint8_t tt = static_cast<uint8_t>(((t - 80) * 255) / 175);
    const uint16_t a = colorLerp(bg, accentFull, tt);
    const uint16_t m = colorLerp(bg, mutedFull, tt);
    displayDrawUtf8Centered("FlinT", UiTok::splashTitleY(SCR_H), a, flintFont(FlintFontId::Body));
    displayDrawUtf8Centered("DeeTech Labs", UiTok::splashSubY(SCR_H), m,
                            flintFont(FlintFontId::Body));
  }

  if (fullPresent) {
    displayPresent();
  } else {
    displayPresentRect(band.x, band.y, band.w, band.h);
  }
}

void splashShow() {
  if (!displayHasCanvas()) {
    tft.fillScreen(rgb565(8, 12, 20));
    flintDelay(UiTok::kSplashTotalMs);
    return;
  }

  const uint32_t t0 = flintMillis();
  uint32_t lastFrame = 0;
  bool paintedFinal = false;

  while (true) {
    const uint32_t elapsed = flintMillis() - t0;
    if (elapsed >= UiTok::kSplashTotalMs) {
      break;
    }

    uint8_t t = 255;
    if (elapsed < UiTok::kSplashAnimMs) {
      t = animEaseOutCubic(animLinearProgress(elapsed, UiTok::kSplashAnimMs));
    }

    if (elapsed == 0 || (elapsed - lastFrame) >= UiTok::kSplashFrameMs || t == 255) {
      // First frame clears the whole FB; later frames only refresh the content band.
      splashPaint(t, /*fullPresent=*/elapsed == 0);
      lastFrame = elapsed;
      paintedFinal = (t >= 255);
      if (t >= 255) {
        const uint32_t remain = UiTok::kSplashTotalMs - elapsed;
        if (remain > 0) {
          flintDelay(remain);
        }
        break;
      }
    }

    flintDelay(5);
  }

  if (!paintedFinal) {
    splashPaint(255, /*fullPresent=*/false);
  }
}
