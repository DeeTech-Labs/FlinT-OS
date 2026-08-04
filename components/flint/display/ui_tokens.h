#pragma once

#include <stdint.h>

// Geometry & motion tokens — single source for layout (not colors).
// Colors live in UiPalette / day_mode.

namespace UiTok {

// —— Spacing ——
constexpr int kPadX = 12;
constexpr int kPadY = 8;
constexpr int kGapSm = 4;
constexpr int kGapMd = 8;

// —— Radii ——
constexpr int kRadiusSm = 6;
constexpr int kRadiusMd = 10;

// —— Face zones (320×240 landscape) ——
// Weather + daily range stack without overlap (icon/temp row, then min/max).
constexpr int kStatusH = 44;
constexpr int kStatusHairlineY = 43;

constexpr int kWxX = 8;
constexpr int kWxY = 2;
constexpr int kWxW = 112;
constexpr int kWxH = 22;

constexpr int kRangeX = 8;
constexpr int kRangeY = 24;
constexpr int kRangeW = 112;
constexpr int kRangeH = 16;

constexpr int kWifiSize = 16;
constexpr int kWifiRightPad = 10;
constexpr int kWifiTop = 10;

constexpr int kModeLabelY = 18;  // baseline in top status row (above daily range)

constexpr int kTimeY = 122;      // hero time baseline
constexpr int kDateY = 158;      // date baseline under time
constexpr int kDateBandH = 22;

// —— Shell overlays ——
constexpr int kShellHeaderH = 36;
constexpr int kShellFooterH = 28;
constexpr int kShellRowH = 26;
constexpr int kShellRowsTop = 40;   // top of first row fill rect
constexpr int kShellSelBarW = 3;
constexpr int kShellScrollW = 3;
constexpr int kShellScrollGap = 4;  // from right edge
constexpr int kShellVisibleRows = 6;
constexpr int kShellHeaderBaseline = 24;
constexpr int kShellUnderlineY = 30;

// —— Splash ——
constexpr int kSplashMarkCy = 92;
constexpr int kSplashTitleY = 152;
constexpr int kSplashSubY = 180;
constexpr int kSplashMarkMin = 40;
constexpr int kSplashMarkMax = 56;

// —— Motion (ms) ——
constexpr uint32_t kAnimOverlayInMs = 200;
constexpr uint32_t kAnimSelectMs = 80;
constexpr uint32_t kSplashTotalMs = 3000;
constexpr uint32_t kSplashAnimMs = 900;
constexpr uint32_t kSplashFrameMs = 33;
constexpr uint32_t kColonPulsePeriodMs = 1000;

inline int wifiX(int screenW) {
  return screenW - kWifiRightPad - kWifiSize;
}

inline int shellFooterBaseline(int screenH) {
  return screenH - 12;
}

inline int shellRowFillY(int visIndex) {
  return kShellRowsTop + visIndex * kShellRowH;
}

inline int shellRowBaseline(int visIndex) {
  return shellRowFillY(visIndex) + 18;
}

}  // namespace UiTok
