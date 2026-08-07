#include "faces/face_digital_layout.h"

#include "display/ui_tokens.h"
#include "util/flint_sys.h"

void DigitalTimeLayout::ensure(const FaceContext& ctx, bool wantSeconds, bool hour12,
                               const lgfx::IFont* timeFont) {
  if (ready && withSeconds == wantSeconds) {
    return;
  }

  auto& gfx = displayGfx();
  displaySetFont(gfx, timeFont);
  digitW = 0;
  for (char c = '0'; c <= '9'; c++) {
    char t[2] = {c, 0};
    const int w = displayTextWidth(gfx, t);
    glyphWDigit[c - '0'] = w;
    if (w > digitW) {
      digitW = w;
    }
  }
  {
    char t[2] = {'-', 0};
    glyphWDash = displayTextWidth(gfx, t);
    if (glyphWDash > digitW) {
      digitW = glyphWDash;
    }
  }
  colonW = displayTextWidth(gfx, ":");
  if (colonW < 1) {
    colonW = digitW / 3;
  }
  ascent = displayFontAscent(gfx);
  glyphH = ascent - displayFontDescent(gfx) + UiTok::kTimeGlyphPadY;
  withSeconds = wantSeconds;
  len = wantSeconds ? 8 : 5;

  const int digits = wantSeconds ? 6 : 4;
  const int colons = wantSeconds ? 2 : 1;
  const int ampmReserve = hour12 ? UiTok::kAmpmLayoutReserve : 0;
  const int avail = UiTok::contentWidth(ctx.screenW) - ampmReserve;
  int total = digitW * digits + colonW * colons;
  if (total > avail && digits > 0) {
    const int over = total - avail;
    const int shrink = (over + digits - 1) / digits;
    digitW -= shrink;
    if (digitW < UiTok::kMinTimeDigitW) {
      digitW = UiTok::kMinTimeDigitW;
    }
    total = digitW * digits + colonW * colons;
  }
  int x = (ctx.screenW - total - ampmReserve) / 2;
  if (x < UiTok::contentLeft()) {
    x = UiTok::contentLeft();
  }
  for (int i = 0; i < len; i++) {
    cellX[i] = x;
    const bool colonSlot = wantSeconds ? (i == 2 || i == 5) : (i == 2);
    x += colonSlot ? colonW : digitW;
  }
  endX = x;
  ready = true;
}

DisplayRect DigitalTimeLayout::timeBand(int screenH) const {
  const int x = ready ? cellX[0] : UiTok::contentLeft();
  const int w = ready ? (endX - x) : 0;
  const int h = glyphH > 0 ? glyphH + UiTok::kTimeGlyphPadY : UiTok::kTimeBandFallbackH;
  return DisplayRect{static_cast<int16_t>(x),
                     static_cast<int16_t>(UiTok::timeY(screenH) - ascent),
                     static_cast<int16_t>(w > 0 ? w : 1), static_cast<int16_t>(h)};
}

DisplayRect DigitalTimeLayout::ampmBand(int screenH) const {
  const int x = endX + UiTok::kGapSm;
  const int h = glyphH > 0 ? glyphH + UiTok::kTimeGlyphPadY : UiTok::kAmpmBandFallbackH;
  return DisplayRect{static_cast<int16_t>(x),
                     static_cast<int16_t>(UiTok::timeY(screenH) - ascent),
                     static_cast<int16_t>(UiTok::kAmpmSlotW), static_cast<int16_t>(h)};
}

int DigitalTimeLayout::appendColonRects(int screenH, const char* timeStr, DisplayRect* out,
                                        int cap) const {
  int n = 0;
  const int ty = UiTok::timeY(screenH) - ascent;
  const int h = glyphH > 0 ? glyphH + UiTok::kTimeGlyphPadY : UiTok::kTimeBandFallbackH;
  for (int i = 0; timeStr[i] != '\0' && n < cap; i++) {
    if (timeStr[i] != ':') {
      continue;
    }
    out[n++] = DisplayRect{static_cast<int16_t>(cellX[i]), static_cast<int16_t>(ty),
                           static_cast<int16_t>(colonW > 0 ? colonW : digitW),
                           static_cast<int16_t>(h)};
  }
  return n;
}

void DigitalTimeLayout::drawGlyph(lgfx::LGFXBase& gfx, int index, char ch, uint16_t fg,
                                  uint16_t bg, int screenH, const lgfx::IFont* timeFont) const {
  displaySetFont(gfx, timeFont);
  const bool isColon = (ch == ':');
  const int cellW = isColon ? colonW : digitW;
  const int x = cellX[index];
  const int ty = UiTok::timeY(screenH);
  gfx.fillRect(x, ty - ascent, cellW, glyphH, bg);
  char tmp[2] = {ch, 0};
  int gw = colonW;
  if (!isColon) {
    if (ch >= '0' && ch <= '9') {
      gw = glyphWDigit[ch - '0'];
    } else if (ch == '-') {
      gw = glyphWDash;
    } else {
      gw = digitW;
    }
  }
  gfx.setTextColor(fg, bg);
  gfx.drawString(tmp, x + (cellW - gw) / 2, ty);
}

DisplayRect digitalDateBand(const FaceContext& ctx, const char* dateStr, int* ioPresentW,
                            const lgfx::IFont* bodyFont) {
  auto& gfx = displayGfx();
  displaySetFont(gfx, bodyFont);
  const int tw = displayTextWidth(gfx, dateStr);
  const int bandH = UiTok::dateBandH(ctx.screenH);
  const int lastPresent = ioPresentW ? *ioPresentW : 0;
  int presentW = tw + UiTok::kDatePresentPadX;
  if (presentW < lastPresent) {
    presentW = lastPresent;
  }
  if (presentW > ctx.screenW) {
    presentW = ctx.screenW;
  }
  int x = (ctx.screenW - presentW) / 2;
  if (x < 0) {
    x = 0;
  }
  if (ioPresentW) {
    *ioPresentW = tw + UiTok::kDatePresentPadX;
  }
  return DisplayRect{static_cast<int16_t>(x),
                     static_cast<int16_t>(UiTok::dateY(ctx.screenH) - UiTok::kDateBandAscentPad),
                     static_cast<int16_t>(presentW), static_cast<int16_t>(bandH)};
}

uint8_t digitalColonFadeNow() {
  const uint32_t phase = flintMillis() % UiTok::kColonPulsePeriodMs;
  const uint32_t half = UiTok::kColonPulsePeriodMs / 2;
  uint8_t t;
  if (phase < half) {
    t = static_cast<uint8_t>((phase * 255UL) / half);
  } else {
    t = static_cast<uint8_t>(((UiTok::kColonPulsePeriodMs - phase) * 255UL) / half);
  }
  return static_cast<uint8_t>(140 + (t * 115UL) / 255);
}

uint16_t digitalColonColor(const UiPalette& pal, uint8_t fade) {
  return colorLerp(pal.textSecondary, pal.accent, fade);
}

bool digitalColonFadeChanged(uint8_t fade, uint8_t lastFade) {
  const int d = static_cast<int>(fade) - static_cast<int>(lastFade);
  return d >= UiTok::kColonFadeQuant || d <= -UiTok::kColonFadeQuant;
}
