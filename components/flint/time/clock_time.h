#pragma once

#include <time.h>
#include <stdint.h>

bool clockTimeIsValid();       // soft (NVS) or NTP — safe to show on face
bool clockTimeIsNtpSynced();   // true only after SNTP
bool clockTimeGetLocal(struct tm* out);
uint32_t clockTimeLastSyncMs();

// Restore last epoch from NVS so UI shows time before Wi‑Fi/NTP.
void clockTimeSoftBegin();

// Non-blocking NTP: begin once, then poll each loop
void clockTimeBeginNtp();
bool clockTimePollNtp();

// Persist current soft/NTP epoch (call on minute tick when valid).
void clockTimePersist();
