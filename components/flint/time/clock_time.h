#pragma once

#include <time.h>
#include <stdint.h>

bool clockTimeIsValid();       // soft (cfg) or NTP — safe to show on face
bool clockTimeIsNtpSynced();   // true only after SNTP
bool clockTimeGetLocal(struct tm* out);
uint32_t clockTimeLastSyncMs();

// Restore last epoch from cfg so UI shows time before Wi‑Fi/NTP.
void clockTimeSoftBegin();

// Non-blocking NTP: begin once, then poll each loop
void clockTimeBeginNtp();
bool clockTimePollNtp();

// Persist current soft/NTP epoch (call on minute tick when valid).
void clockTimePersist();

// Manual soft-clock edit (settings UI). Persists to cfg. Holds until next NTP apply.
bool clockTimeSetLocal(const struct tm* local);
// ±minutes / ±days from current (or default seed if unset); persists.
bool clockTimeAdjustMinutes(int deltaMin);
bool clockTimeAdjustDays(int deltaDays);
