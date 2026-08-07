#include "day_mode_hour.h"

DayMode dayModeFromHour(int hour) {
  if (hour >= kDayModeNightStart || hour < kDayModeMorningStart) {
    return DAY_MODE_NIGHT;
  }
  if (hour < kDayModeDayStart) {
    return DAY_MODE_MORNING;
  }
  if (hour < kDayModeEveningStart) {
    return DAY_MODE_DAY;
  }
  return DAY_MODE_EVENING;
}
