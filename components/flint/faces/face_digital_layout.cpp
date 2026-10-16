#include "faces/face_digital_layout.h"

#include "display/ui_tokens.h"
#include "sdk/resources.h"

#include <string.h>

namespace {

// Glyph metrics depend only on the time font — amortize textWidth across layout
// rebuilds (withSeconds / hour12 / screenW still recompute cellX).
struct TimeFontMetrics {
  const lgfx::IFont* font = nullptr;
  int digitW = 0;
  int colonW = 0;
  int glyphWDigit[10] = {};
  int glyphWDash = 0;
  int ascent = 0;
  int glyphH = 0;
  bool valid = false;
};

TimeFontMetrics gTimeFontMetrics{};

void ensureTimeFontMetrics(const lgfx::IFont* timeFont) {
  if (gTimeFontMetrics.valid && gTimeFontMetrics.font == timeFont) {
    return;
  }
  auto& gfx = displayGfx();
  displaySetFont(gfx, timeFont);
  int digitW = 0;
  for (char c = '0'; c <= '9'; c++) {
    char t[2] = {c, 0};
    const int w = displayTextWidth(gfx, t);
    gTimeFontMetrics.glyphWDigit[c - '0'] = w;
    if (w > digitW) {
      digitW = w;
    }
  }
  {
    char t[2] = {'-', 0};
    gTimeFontMetrics.glyphWDash = displayTextWidth(gfx, t);
    if (gTimeFontMetrics.glyphWDash > digitW) {
      digitW = gTimeFontMetrics.glyphWDash;
    }
  }
  int colonW = displayTextWidth(gfx, ":");
  if (colonW < 1) {
    colonW = digitW / 3;
  }
  gTimeFontMetrics.digitW = digitW;
  gTimeFontMetrics.colonW = colonW;
  gTimeFontMetrics.ascent = displayFontAscent(gfx);
  gTimeFontMetrics.glyphH =
      gTimeFontMetrics.ascent - displayFontDescent(gfx) + UiTok::kTimeGlyphPadY;
  gTimeFontMetrics.font = timeFont;
  gTimeFontMetrics.valid = true;
}

}  // namespace

void DigitalTimeLayout::ensure(const FaceContext& ctx, bool wantSeconds, bool hour12,
                               const lgfx::IFont* timeFont) {
  if (ready && withSeconds == wantSeconds) {
    return;
  }

  ensureTimeFontMetrics(timeFont);
  digitW = gTimeFontMetrics.digitW;
  colonW = gTimeFontMetrics.colonW;
  glyphWDash = gTimeFontMetrics.glyphWDash;
  ascent = gTimeFontMetrics.ascent;
  glyphH = gTimeFontMetrics.glyphH;
  memcpy(glyphWDigit, gTimeFontMetrics.glyphWDigit, sizeof(glyphWDigit));

  withSeconds = wantSeconds;
  len = wantSeconds ? 8 : 5;

  // Hours and minutes own the visual hierarchy; seconds use the body font.
  auto& gfx = displayGfx();
  displaySetFont(gfx, flintFont(FlintFontId::Body));
  const int smallDigitW = displayTextWidth(gfx, "0");
  const int secondsGap = UiTok::kGapMd + UiTok::kGapSm;
  // Keep this slot even with seconds hidden: HH:MM never shifts.
  const int secondsW = secondsGap + smallDigitW * 2;
  const int ampmReserve = hour12 ? UiTok::kAmpmLayoutReserve : 0;
  // Each face size is baked at its native resolution: no bitmap enlargement.
  heroFont = timeFont;
  const int total = digitW * 4 + colonW + secondsW;
  int x = (ctx.screenW - total - ampmReserve) / 2;
  if (x < UiTok::contentLeft()) x = UiTok::contentLeft();
  for (int i = 0; i < len; i++) {
    cellX[i] = x;
    cellW[i] = i == 2 ? colonW : (i == 5 ? secondsGap : (i > 5 ? smallDigitW : digitW));
    x += cellW[i];
  }
  endX = cellX[0] + total;
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
    if (i != 2 || timeStr[i] != ':') {
      continue;
    }
    out[n++] = DisplayRect{static_cast<int16_t>(cellX[i]), static_cast<int16_t>(ty),
                           static_cast<int16_t>(cellW[i]),
                           static_cast<int16_t>(h)};
  }
  return n;
}

int DigitalTimeLayout::appendChangedGlyphRects(int screenH, const char* prev,
                                               const char* next, DisplayRect* out,
                                               int cap) const {
  if (!out || cap <= 0 || !next) {
    return 0;
  }
  const size_t prevLen = prev ? strlen(prev) : 0;
  const size_t nextLen = strlen(next);
  if (prevLen == 0 || prevLen != nextLen || !ready) {
    out[0] = timeBand(screenH);
    return 1;
  }

  const int ty = UiTok::timeY(screenH) - ascent;
  const int h = glyphH > 0 ? glyphH + UiTok::kTimeGlyphPadY : UiTok::kTimeBandFallbackH;
  int n = 0;
  for (size_t i = 0; i < nextLen && n < cap; i++) {
    if (prev[i] == next[i]) {
      continue;
    }
    const int width = cellW[i];
    out[n++] = DisplayRect{static_cast<int16_t>(cellX[i]), static_cast<int16_t>(ty),
                           static_cast<int16_t>(width), static_cast<int16_t>(h)};
  }
  return n;
}

void DigitalTimeLayout::drawGlyph(lgfx::LGFXBase& gfx, int index, char ch, uint16_t fg,
                                  uint16_t bg, int screenH) const {
  const bool small = index >= 5;
  const int width = cellW[index];
  const int x = cellX[index];
  const int ty = UiTok::timeY(screenH);
  gfx.fillRect(x, ty - ascent, width, glyphH, bg);
  // Slot 5 is breathing room between minutes and the secondary seconds.
  if (index == 5) return;
  displaySetFont(gfx, small ? flintFont(FlintFontId::Body) : heroFont);
  gfx.setTextSize(1.0f);
  char tmp[2] = {ch, 0};
  const int gw = displayTextWidth(gfx, tmp);
  gfx.setTextColor(fg, bg);
  gfx.drawString(tmp, x + (width - gw) / 2, ty);
  // Font scale is global LovyanGFX state: never leak it into date/shell text.
  gfx.setTextSize(1.0f);
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
