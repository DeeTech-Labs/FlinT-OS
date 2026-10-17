#pragma once

#include <stdint.h>

#include "board/board_config.inc"

// Geometry & motion tokens — single source for layout (not colors).
// Colors live in UiPalette / day_mode.
//
// Design space comes from the selected board canvas (FLINT_DESIGN_* after
// rotation). Call sites should prefer the helpers below with runtime
// screenW/screenH instead of raw constants when placing content that must
// track resolution.

namespace UiTok {

// —— Design reference (from board canvas) ——
constexpr int kDesignW = FLINT_DESIGN_W;
constexpr int kDesignH = FLINT_DESIGN_H;

inline int mapX(int designX, int screenW) {
  return (designX * screenW) / kDesignW;
}

inline int mapY(int designY, int screenH) {
  return (designY * screenH) / kDesignH;
}

inline int mapW(int designW, int screenW) {
  return (designW * screenW) / kDesignW;
}

inline int mapH(int designH, int screenH) {
  return (designH * screenH) / kDesignH;
}

// —— Safe area (rounded glass) ——
// Keep interactive/text content inside the flat region; backgrounds may still
// fill to the panel edge under the bezel.
constexpr int kSafeInset = FLINT_SAFE_INSET;

// —— Spacing ——
constexpr int kPadX = kSafeInset;
constexpr int kPadY = 10;
constexpr int kGapSm = 4;
constexpr int kGapMd = 8;

// —— Radii ——
constexpr int kRadiusSm = 6;
constexpr int kRadiusMd = 10;

// —— Face zones (design-space coordinates) ——
// Top status: centered mode label.
constexpr int kStatusH = 48;
constexpr int kStatusHairlineY = 48;


constexpr int kModeLabelY = 35;  // baseline in top status row

constexpr int kTimeY = 140;      // hero time baseline
constexpr int kDateY = 185;      // date baseline under time
constexpr int kDateBandH = 22;

// Digital face — time / AM·PM / date dirty-band geometry (design space).
constexpr int kTimeGlyphPadY = 2;
constexpr int kTimeBandFallbackH = 40;
constexpr int kMinTimeDigitW = 8;
constexpr int kAmpmSlotW = 48;         // clear / present band for AM·PM
constexpr int kAmpmLayoutReserve = 42; // horizontal reserve when packing cells
constexpr int kAmpmBandFallbackH = 24;
constexpr int kAmpmBaselineLift = 4;
constexpr int kDatePresentPadX = 16;
constexpr int kDateBandAscentPad = 16;
constexpr int kModeLabelClearY = 17;
constexpr int kModeLabelClearH = 24;
// Colon pulse: ignore fade deltas smaller than this (SPI throttle).
constexpr int kColonFadeQuant = 16;

// —— Shell overlays ——
constexpr int kShellHeaderH = 36;
constexpr int kShellFooterH = 30;
constexpr int kShellRowH = 26;
constexpr int kShellRowsTop = 42;   // top of first row fill rect
constexpr int kShellSelBarW = 3;
constexpr int kShellScrollW = 3;
constexpr int kShellScrollGap = kSafeInset;  // from right edge
constexpr int kShellVisibleRows = 6;
constexpr int kShellHeaderBaseline = 26;
constexpr int kShellUnderlineY = 32;

// —— Splash ——
constexpr int kSplashMarkCy = 92;
constexpr int kSplashTitleY = 152;
constexpr int kSplashSubY = 180;
constexpr int kSplashMarkMin = 40;
constexpr int kSplashMarkMax = 52;

// —— Motion (ms) ——
constexpr uint32_t kAnimOverlayInMs = 200;
constexpr uint32_t kAnimSelectMs = 80;
constexpr uint32_t kSplashTotalMs = 1600;
constexpr uint32_t kSplashAnimMs = 550;
constexpr uint32_t kSplashFrameMs = 40;
constexpr uint32_t kColonPulsePeriodMs = 1000;
// Shell motion dirty cadence while seconds (colon pulse) are active.
constexpr uint32_t kColonPulseTickMs = 90;

inline int contentLeft() { return kSafeInset; }
inline int contentRight(int screenW) { return screenW - kSafeInset; }
inline int contentTop() { return kSafeInset; }
inline int contentBottom(int screenH) { return screenH - kSafeInset; }
inline int contentWidth(int screenW) { return screenW - kSafeInset * 2; }

inline int statusH(int screenH) { return mapH(kStatusH, screenH); }
inline int statusHairlineY(int screenH) { return mapY(kStatusHairlineY, screenH); }
inline int timeY(int screenH) { return mapY(kTimeY, screenH); }
inline int dateY(int screenH) { return mapY(kDateY, screenH); }
inline int dateBandH(int screenH) { return mapH(kDateBandH, screenH); }
inline int modeLabelY(int screenH) { return mapY(kModeLabelY, screenH); }

inline int splashMarkCy(int screenH) { return mapY(kSplashMarkCy, screenH); }
inline int splashTitleY(int screenH) { return mapY(kSplashTitleY, screenH); }
inline int splashSubY(int screenH) { return mapY(kSplashSubY, screenH); }

// Center band for mode label.
inline int modeLabelMaxW(int screenW) {
  const int left = contentLeft() + kGapMd;
  const int right = contentRight(screenW) - kGapMd;
  const int w = right - left;
  return w > 40 ? w : 40;
}

inline int shellFooterBaseline(int screenH) {
  return contentBottom(screenH) - 2;
}

inline int shellRowFillY(int visIndex) {
  return kShellRowsTop + visIndex * kShellRowH;
}

inline int shellRowBaseline(int visIndex) {
  return shellRowFillY(visIndex) + 18;
}

}  // namespace UiTok
