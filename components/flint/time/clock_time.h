#pragma once

#include <time.h>
#include <stdint.h>

bool clockTimeIsValid();       // soft (cfg) or PC — safe to show on face
bool clockTimeIsPcSynced();    // successful PC sync in this boot; manual edit clears it
bool clockTimeGetLocal(struct tm* out);
// Deadline for the next visible clock tick (1..1000 ms).
uint32_t clockTimeNextTickMs();

// Restore last epoch from cfg so UI shows time before USB sync.
void clockTimeSoftBegin();

// Set UTC epoch received from the host PC; persists immediately.
bool clockTimeSyncFromPc(time_t epoch);

// Persist current soft/PC epoch (call on minute tick when valid).
void clockTimePersist();

// Manual soft-clock edit (settings UI). Persists to cfg. PC sync may replace it.
bool clockTimeSetLocal(const struct tm* local);
// ±minutes / ±days from current (or default seed if unset); persists.
bool clockTimeAdjustMinutes(int deltaMin);
bool clockTimeAdjustDays(int deltaDays);
