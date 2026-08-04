#include "ui/chrome.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "sdk/resources.h"

#include <stdio.h>

static uint16_t chromeFade(uint16_t bg, uint16_t fg, uint8_t fade) {
  return colorLerp(bg, fg, fade);
}

void chromeFillBackdrop(const FaceContext& ctx) {
  displayGfx().fillScreen(ctx.palette.bgTop);
}

void chromeHeader(const FaceContext& ctx, const char* title, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const uint16_t titleC = chromeFade(bg, p.accent, fade);
  const uint16_t lineC = chromeFade(bg, p.divider, fade);

  displayDrawText(gfx, title, UiTok::kPadX, UiTok::kShellHeaderBaseline, titleC, bg,
                  flintFont(FlintFontId::Body));
  gfx.drawFastHLine(UiTok::kPadX, UiTok::kShellUnderlineY,
                    ctx.screenW - UiTok::kPadX * 2 - UiTok::kShellScrollGap - 8, lineC);
}

void chromeFooter(const FaceContext& ctx, const char* hint, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const uint16_t muted = chromeFade(bg, p.footerMuted, fade);
  const uint16_t lineC = chromeFade(bg, p.divider, fade);
  const int yLine = ctx.screenH - UiTok::kShellFooterH;

  gfx.drawFastHLine(UiTok::kPadX, yLine, ctx.screenW - UiTok::kPadX * 2, lineC);
  displayDrawText(gfx, hint, UiTok::kPadX, UiTok::shellFooterBaseline(ctx.screenH), muted, bg,
                  flintFont(FlintFontId::Body));
}

void chromeListRow(const FaceContext& ctx, int rowTopY, int rowH, const char* label,
                   bool selected, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const int textPad = UiTok::kPadX + UiTok::kShellSelBarW + UiTok::kGapSm;
  const int rowW = ctx.screenW - UiTok::kShellScrollGap - UiTok::kShellScrollW - 2;

  if (selected) {
    const uint16_t selBg = chromeFade(bg, p.selectionBg, fade);
    const uint16_t bar = chromeFade(bg, p.accent, fade);
    gfx.fillRect(0, rowTopY, rowW, rowH, selBg);
    gfx.fillRect(0, rowTopY + 2, UiTok::kShellSelBarW, rowH - 4, bar);
  } else {
    gfx.fillRect(0, rowTopY, rowW, rowH, bg);
  }

  const uint16_t fgTarget = selected ? p.selectionFg : p.textSecondary;
  const uint16_t fg = chromeFade(bg, fgTarget, fade);
  const uint16_t textBg = selected ? chromeFade(bg, p.selectionBg, fade) : bg;
  const int baseline = rowTopY + rowH - 8;
  displayDrawText(gfx, label, textPad, baseline, fg, textBg, flintFont(FlintFontId::Body));
}

DisplayRect chromeListRowRect(const FaceContext& ctx, int rowTopY, int rowH) {
  return DisplayRect{0, static_cast<int16_t>(rowTopY),
                     static_cast<int16_t>(ctx.screenW - UiTok::kShellScrollGap),
                     static_cast<int16_t>(rowH)};
}

void chromeScrollTrack(const FaceContext& ctx, int trackY, int trackH, int thumbY,
                       int thumbH) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const int x = ctx.screenW - UiTok::kShellScrollGap - UiTok::kShellScrollW;
  gfx.fillRect(x, trackY, UiTok::kShellScrollW, trackH, p.panel);
  const int h = thumbH > 4 ? thumbH : 4;
  gfx.fillRect(x, thumbY, UiTok::kShellScrollW, h, p.accent);
}

void chromeCounter(const FaceContext& ctx, int cur, int total, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const uint16_t fg = chromeFade(bg, p.textSecondary, fade);

  char cue[16];
  snprintf(cue, sizeof(cue), "%d/%d", cur, total);
  displaySetFont(gfx, flintFont(FlintFontId::Body));
  const int tw = displayTextWidth(gfx, cue);
  const int x = ctx.screenW - tw - UiTok::kPadX;
  gfx.fillRect(x - 4, 6, tw + 8, 20, bg);
  displayDrawText(gfx, cue, x, UiTok::kShellHeaderBaseline, fg, bg, flintFont(FlintFontId::Body));
}
