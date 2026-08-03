#pragma once

#include <time.h>
#include <stdint.h>

bool clockTimeIsValid();
bool clockTimeGetLocal(struct tm* out);
uint32_t clockTimeLastSyncMs();

// Non-blocking NTP: begin once, then poll each loop
void clockTimeBeginNtp();
bool clockTimePollNtp();
