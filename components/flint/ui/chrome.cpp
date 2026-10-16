#include "ui/chrome.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "sdk/resources.h"

#include <stdio.h>
#include <string.h>

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
  const int left = UiTok::contentLeft();
  const int width = UiTok::contentWidth(ctx.screenW) - UiTok::kShellScrollW - UiTok::kGapSm;

  displayDrawText(gfx, title, left, UiTok::kShellHeaderBaseline, titleC, bg,
                  flintFont(FlintFontId::Body));
  gfx.drawFastHLine(left, UiTok::kShellUnderlineY, width, lineC);
}

void chromeFooter(const FaceContext& ctx, const char* hint, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const uint16_t muted = chromeFade(bg, p.footerMuted, fade);
  const uint16_t lineC = chromeFade(bg, p.divider, fade);
  const int left = UiTok::contentLeft();
  const int yLine = ctx.screenH - UiTok::kShellFooterH - UiTok::kGapMd;

  gfx.drawFastHLine(left, yLine, UiTok::contentWidth(ctx.screenW), lineC);
  displayDrawText(gfx, hint, left, UiTok::shellFooterBaseline(ctx.screenH), muted, bg,
                  flintFont(FlintFontId::Body));
}

void chromeListRow(const FaceContext& ctx, int rowTopY, int rowH, const char* label,
                   bool selected, uint8_t fade) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const uint16_t bg = p.bgTop;
  const int left = UiTok::contentLeft();
  const int textPad = left + UiTok::kShellSelBarW + UiTok::kGapSm;
  const int rowW = UiTok::contentRight(ctx.screenW) - UiTok::kShellScrollW - left;

  gfx.fillRect(left, rowTopY, rowW, rowH, bg);
  if (selected) {
    const uint16_t selBg = chromeFade(bg, p.selectionBg, fade);
    const uint16_t bar = chromeFade(bg, p.accent, fade);
    gfx.fillRoundRect(left, rowTopY + 1, rowW, rowH - 2, UiTok::kRadiusSm, selBg);
    gfx.fillRoundRect(left + 2, rowTopY + 7, UiTok::kShellSelBarW, rowH - 14, 1, bar);
  } else {
    gfx.fillRect(left, rowTopY, rowW, rowH, bg);
  }

  const uint16_t fgTarget = selected ? p.selectionFg : p.textSecondary;
  const uint16_t fg = chromeFade(bg, fgTarget, fade);
  const uint16_t textBg = selected ? chromeFade(bg, p.selectionBg, fade) : bg;
  const int baseline = rowTopY + rowH - 8;
  displayDrawText(gfx, label, textPad, baseline, fg, textBg, flintFont(FlintFontId::Body));
}

void chromeValueRow(const FaceContext& ctx, int rowTopY, int rowH,
                    const char* label, const char* value, bool selected, uint8_t fade) {
  chromeListRow(ctx, rowTopY, rowH, "", selected, fade);
  auto& gfx = displayGfx();
  displaySetFont(gfx, flintFont(FlintFontId::Body));
  const int left = UiTok::contentLeft() + UiTok::kShellSelBarW + UiTok::kGapSm;
  const int right = UiTok::contentRight(ctx.screenW) - UiTok::kShellScrollW - UiTok::kGapSm;
  const int labelW = displayTextWidth(gfx, label);
  const int available = right - left - labelW - UiTok::kGapMd;
  char fitted[96];
  snprintf(fitted, sizeof(fitted), "%s", value);
  while (fitted[0] && displayTextWidth(gfx, fitted) > available) {
    size_t n = strlen(fitted);
    // Remove a previous ellipsis, then one complete UTF-8 character.
    if (n >= 3 && strcmp(fitted + n - 3, "…") == 0) n -= 3;
    if (n == 0) { fitted[0] = '\0'; break; }
    --n;
    while (n > 0 && (static_cast<unsigned char>(fitted[n]) & 0xc0) == 0x80) --n;
    snprintf(fitted + n, sizeof(fitted) - n, "…");
  }
  const uint16_t bg = selected ? chromeFade(ctx.palette.bgTop, ctx.palette.selectionBg, fade)
                                : ctx.palette.bgTop;
  const int baseline = rowTopY + rowH - 8;
  displayDrawText(gfx, label, left, baseline,
                  chromeFade(ctx.palette.bgTop, ctx.palette.textSecondary, fade), bg,
                  flintFont(FlintFontId::Body));
  displayDrawText(gfx, fitted, right - displayTextWidth(gfx, fitted), baseline,
                  chromeFade(ctx.palette.bgTop, ctx.palette.textPrimary, fade), bg,
                  flintFont(FlintFontId::Body));
}

DisplayRect chromeListRowRect(const FaceContext& ctx, int rowTopY, int rowH) {
  const int left = UiTok::contentLeft();
  return DisplayRect{static_cast<int16_t>(left), static_cast<int16_t>(rowTopY),
                     static_cast<int16_t>(UiTok::contentRight(ctx.screenW) - left),
                     static_cast<int16_t>(rowH)};
}

void chromeScrollTrack(const FaceContext& ctx, int trackY, int trackH, int thumbY,
                       int thumbH) {
  auto& gfx = displayGfx();
  const UiPalette& p = ctx.palette;
  const int x = UiTok::contentRight(ctx.screenW) - UiTok::kShellScrollW;
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
  const int x = UiTok::contentRight(ctx.screenW) - tw;
  gfx.fillRect(x - 4, UiTok::contentTop() - 2, tw + 8, 20, bg);
  displayDrawText(gfx, cue, x, UiTok::kShellHeaderBaseline, fg, bg, flintFont(FlintFontId::Body));
}
