#pragma once

#include <stdint.h>

// Geometry & motion tokens — single source for layout (not colors).
// Colors live in UiPalette / day_mode.
//
// Design space is landscape 280×240 (panel 240×280 @ rotation). Call sites should
// prefer the helpers below with runtime screenW/screenH instead of raw constants
// when placing content that must track resolution.

namespace UiTok {

// —— Design reference (landscape) ——
constexpr int kDesignW = 280;
constexpr int kDesignH = 240;

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
constexpr int kSafeInset = 16;

// —— Spacing ——
constexpr int kPadX = kSafeInset;
constexpr int kPadY = 10;
constexpr int kGapSm = 4;
constexpr int kGapMd = 8;

// —— Radii ——
constexpr int kRadiusSm = 6;
constexpr int kRadiusMd = 10;

// —— Face zones (design-space 280×240) ——
// Weather + daily range stack without overlap (icon/temp row, then min/max).
constexpr int kStatusH = 48;
constexpr int kStatusHairlineY = 47;

constexpr int kWxX = kSafeInset;
constexpr int kWxY = 10;
constexpr int kWxW = 96;
constexpr int kWxH = 22;

constexpr int kRangeX = kSafeInset;
constexpr int kRangeY = 30;
constexpr int kRangeW = 96;
constexpr int kRangeH = 16;

constexpr int kWifiSize = 16;
constexpr int kWifiRightPad = kSafeInset;
constexpr int kWifiTop = 12;

constexpr int kModeLabelY = 22;  // baseline in top status row (above daily range)

constexpr int kTimeY = 124;      // hero time baseline
constexpr int kDateY = 160;      // date baseline under time
constexpr int kDateBandH = 22;

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
constexpr uint32_t kSplashTotalMs = 3000;
constexpr uint32_t kSplashAnimMs = 900;
constexpr uint32_t kSplashFrameMs = 33;
constexpr uint32_t kColonPulsePeriodMs = 1000;

inline int contentLeft() { return kSafeInset; }
inline int contentRight(int screenW) { return screenW - kSafeInset; }
inline int contentTop() { return kSafeInset; }
inline int contentBottom(int screenH) { return screenH - kSafeInset; }
inline int contentWidth(int screenW) { return screenW - kSafeInset * 2; }

inline int wifiX(int screenW) {
  return screenW - kWifiRightPad - kWifiSize;
}

inline int statusH(int screenH) { return mapH(kStatusH, screenH); }
inline int statusHairlineY(int screenH) { return mapY(kStatusHairlineY, screenH); }
inline int timeY(int screenH) { return mapY(kTimeY, screenH); }
inline int dateY(int screenH) { return mapY(kDateY, screenH); }
inline int dateBandH(int screenH) { return mapH(kDateBandH, screenH); }
inline int modeLabelY(int screenH) { return mapY(kModeLabelY, screenH); }
inline int wifiTop(int screenH) { return mapY(kWifiTop, screenH); }

inline int wxX(int screenW) { return mapX(kWxX, screenW); }
inline int wxY(int screenH) { return mapY(kWxY, screenH); }
inline int wxW(int screenW) { return mapW(kWxW, screenW); }
inline int wxH(int screenH) { return mapH(kWxH, screenH); }
inline int rangeX(int screenW) { return mapX(kRangeX, screenW); }
inline int rangeY(int screenH) { return mapY(kRangeY, screenH); }
inline int rangeW(int screenW) { return mapW(kRangeW, screenW); }
inline int rangeH(int screenH) { return mapH(kRangeH, screenH); }

inline int splashMarkCy(int screenH) { return mapY(kSplashMarkCy, screenH); }
inline int splashTitleY(int screenH) { return mapY(kSplashTitleY, screenH); }
inline int splashSubY(int screenH) { return mapY(kSplashSubY, screenH); }

// Center band between weather block and wifi icon (for mode label).
inline int modeLabelMaxW(int screenW) {
  const int left = wxX(screenW) + wxW(screenW) + kGapMd;
  const int right = wifiX(screenW) - kGapMd;
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
