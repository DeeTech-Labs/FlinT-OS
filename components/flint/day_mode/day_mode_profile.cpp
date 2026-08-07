#include "day_mode_profile.h"

const DayModeProfile& dayModeProfile(DayMode mode) {
  static const DayModeProfile kProfiles[] = {
      // seconds, label, wash
      {true, true, 64},    // morning
      {true, true, 52},    // day
      {true, true, 72},    // evening
      {false, true, 36},   // night
      {false, false, 40},  // waiting (!hasTime): no auto-seconds; date carries waiting copy
  };
  switch (mode) {
    case DAY_MODE_MORNING:
      return kProfiles[0];
    case DAY_MODE_DAY:
      return kProfiles[1];
    case DAY_MODE_EVENING:
      return kProfiles[2];
    case DAY_MODE_NIGHT:
      return kProfiles[3];
    case DAY_MODE_WAITING:
    default:
      return kProfiles[4];
  }
}
