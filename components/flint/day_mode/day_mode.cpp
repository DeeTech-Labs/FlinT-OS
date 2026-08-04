#include "day_mode.h"
#include "config/config.h"
#include "display/display.h"
#include "i18n/i18n.h"

DayMode dayModeFromHour(int hour) {
  // [nightStart..24) U [0..morningStart) = ночь
  if (hour >= DAY_MODE_NIGHT_START || hour < DAY_MODE_MORNING_START) {
    return DAY_MODE_NIGHT;
  }
  if (hour < DAY_MODE_DAY_START) {
    return DAY_MODE_MORNING;
  }
  if (hour < DAY_MODE_EVENING_START) {
    return DAY_MODE_DAY;
  }
  return DAY_MODE_EVENING;
}

const DayModeProfile& dayModeProfile(DayMode mode) {
  static const DayModeProfile kProfiles[] = {
      // seconds, weather, label, wash
      {true, true, true, 64},   // morning
      {true, true, true, 52},   // day
      {true, true, true, 72},   // evening
      {false, true, true, 36},  // night
  };
  switch (mode) {
    case DAY_MODE_MORNING: return kProfiles[0];
    case DAY_MODE_DAY: return kProfiles[1];
    case DAY_MODE_EVENING: return kProfiles[2];
    case DAY_MODE_NIGHT:
    default: return kProfiles[3];
  }
}

const char* dayModeName(DayMode mode) {
  return flintTrDayMode(static_cast<uint8_t>(mode));
}

const char* dayModeNameRu(DayMode mode) {
  return dayModeName(mode);
}

static uint16_t weatherAccent(int code, uint16_t base) {
  if (code == 0) return rgb565(255, 200, 70);
  if (code <= 3) return rgb565(160, 190, 220);
  if (code == 45 || code == 48) return rgb565(170, 180, 190);
  if (code >= 51 && code <= 67) return rgb565(80, 160, 255);
  if (code >= 71 && code <= 86) return rgb565(200, 230, 255);
  if (code >= 80 && code <= 82) return rgb565(70, 140, 255);
  if (code >= 95) return rgb565(180, 120, 255);
  return base;
}

static void fillChromeRoles(UiPalette& p) {
  p.divider = colorLerp(p.bgTop, p.accent, 70);
  p.selectionBg = colorLerp(p.bgTop, p.panel, 200);
  p.selectionFg = p.textPrimary;
  p.footerMuted = colorLerp(p.bgTop, p.textSecondary, 180);
  p.glow = p.accent;
  p.panelBorder = colorLerp(p.panel, p.textSecondary, 60);
}

UiPalette dayModeBuildPalette(DayMode mode, int weatherCode, bool weatherOk) {
  UiPalette p{};
  p.wifiOn = rgb565(90, 220, 150);
  p.wifiOff = rgb565(255, 95, 95);

  switch (mode) {
    case DAY_MODE_MORNING:
      // тёплый рассвет
      p.bgTop = rgb565(18, 12, 14);
      p.bgBottom = rgb565(52, 30, 24);
      p.accent = rgb565(255, 168, 88);
      p.textPrimary = rgb565(255, 248, 240);
      p.textSecondary = rgb565(196, 158, 128);
      p.panel = rgb565(38, 26, 26);
      break;

    case DAY_MODE_DAY:
      // ясный дневной teal
      p.bgTop = rgb565(8, 12, 20);
      p.bgBottom = rgb565(16, 34, 46);
      p.accent = rgb565(72, 224, 214);
      p.textPrimary = rgb565(244, 250, 255);
      p.textSecondary = rgb565(132, 158, 178);
      p.panel = rgb565(20, 32, 42);
      break;

    case DAY_MODE_EVENING:
      // закат / магента
      p.bgTop = rgb565(16, 8, 20);
      p.bgBottom = rgb565(46, 16, 34);
      p.accent = rgb565(255, 118, 168);
      p.textPrimary = rgb565(255, 240, 248);
      p.textSecondary = rgb565(186, 138, 166);
      p.panel = rgb565(34, 20, 34);
      break;

    case DAY_MODE_NIGHT:
    default:
      // спокойная ночь
      p.bgTop = rgb565(3, 5, 10);
      p.bgBottom = rgb565(6, 10, 18);
      p.accent = rgb565(86, 122, 176);
      p.textPrimary = rgb565(168, 184, 208);
      p.textSecondary = rgb565(78, 94, 118);
      p.panel = rgb565(10, 14, 24);
      p.wifiOn = rgb565(48, 128, 92);
      p.wifiOff = rgb565(128, 56, 56);
      break;
  }

  fillChromeRoles(p);

  // Ночью погода почти не красит акцент (бережём «тихий» режим)
  if (weatherOk && mode != DAY_MODE_NIGHT) {
    p.accent = colorLerp(p.accent, weatherAccent(weatherCode, p.accent), 140);
    p.glow = p.accent;
    p.divider = colorLerp(p.bgTop, p.accent, 70);
  } else if (weatherOk && mode == DAY_MODE_NIGHT) {
    p.accent = colorLerp(p.accent, weatherAccent(weatherCode, p.accent), 40);
    p.glow = p.accent;
    p.divider = colorLerp(p.bgTop, p.accent, 50);
  }

  return p;
}
