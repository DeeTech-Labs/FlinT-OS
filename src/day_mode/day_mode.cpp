#include "day_mode.h"
#include "src/config/config.h"
#include "src/display/display.h"

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
      // mode, name, seconds, weather, label, wash
      {DAY_MODE_MORNING, "утро", true, true, true, 70},
      {DAY_MODE_DAY, "день", true, true, true, 55},
      {DAY_MODE_EVENING, "вечер", true, true, true, 80},
      {DAY_MODE_NIGHT, "ночь", false, true, true, 40},
  };
  switch (mode) {
    case DAY_MODE_MORNING: return kProfiles[0];
    case DAY_MODE_DAY: return kProfiles[1];
    case DAY_MODE_EVENING: return kProfiles[2];
    case DAY_MODE_NIGHT:
    default: return kProfiles[3];
  }
}

const char* dayModeNameRu(DayMode mode) {
  return dayModeProfile(mode).nameRu;
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

UiPalette dayModeBuildPalette(DayMode mode, int weatherCode, bool weatherOk) {
  UiPalette p{};
  p.wifiOn = rgb565(90, 220, 150);
  p.wifiOff = rgb565(255, 95, 95);
  p.panelBorder = rgb565(255, 255, 255);
  p.panel = rgb565(24, 28, 40);

  switch (mode) {
    case DAY_MODE_MORNING:
      // тёплый рассвет
      p.bgTop = rgb565(22, 14, 16);
      p.bgBottom = rgb565(48, 28, 22);
      p.accent = rgb565(255, 170, 90);
      p.textPrimary = rgb565(255, 248, 240);
      p.textSecondary = rgb565(200, 160, 130);
      p.panel = rgb565(40, 28, 28);
      break;

    case DAY_MODE_DAY:
      // ясный дневной
      p.bgTop = rgb565(10, 16, 24);
      p.bgBottom = rgb565(18, 36, 48);
      p.accent = rgb565(80, 230, 220);
      p.textPrimary = rgb565(245, 250, 255);
      p.textSecondary = rgb565(140, 165, 185);
      p.panel = rgb565(22, 34, 44);
      break;

    case DAY_MODE_EVENING:
      // закат / магента
      p.bgTop = rgb565(20, 10, 24);
      p.bgBottom = rgb565(48, 18, 36);
      p.accent = rgb565(255, 120, 170);
      p.textPrimary = rgb565(255, 240, 248);
      p.textSecondary = rgb565(190, 140, 170);
      p.panel = rgb565(36, 22, 36);
      break;

    case DAY_MODE_NIGHT:
    default:
      // спокойная ночь, меньше контраста
      p.bgTop = rgb565(4, 6, 12);
      p.bgBottom = rgb565(8, 12, 22);
      p.accent = rgb565(90, 130, 190);
      p.textPrimary = rgb565(180, 195, 220);
      p.textSecondary = rgb565(90, 105, 130);
      p.panel = rgb565(12, 16, 28);
      p.wifiOn = rgb565(50, 140, 100);
      p.wifiOff = rgb565(140, 60, 60);
      break;
  }

  p.glow = p.accent;

  // Ночью погода почти не красит акцент (бережём «тихий» режим)
  if (weatherOk && mode != DAY_MODE_NIGHT) {
    p.accent = colorLerp(p.accent, weatherAccent(weatherCode, p.accent), 140);
    p.glow = p.accent;
  } else if (weatherOk && mode == DAY_MODE_NIGHT) {
    p.accent = colorLerp(p.accent, weatherAccent(weatherCode, p.accent), 40);
    p.glow = p.accent;
  }

  return p;
}
