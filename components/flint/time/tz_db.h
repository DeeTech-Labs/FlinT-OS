#pragma once

#include <stddef.h>
#include <stdint.h>

// Curated IANA ids + POSIX TZ for setenv. Not a full zoneinfo DB.

struct TzEntry {
  const char* id;     // e.g. "Europe/Moscow" (Open-Meteo)
  const char* posix;  // e.g. "MSK-3"
  const char* label;  // short UI label (ASCII)
};

int tzDbCount();
const TzEntry* tzDbAt(int index);
const TzEntry* tzDbFind(const char* id);  // fallback Moscow if unknown
int tzDbIndexOf(const char* id);          // -1 if unknown
void tzDbUrlEncodeId(const char* id, char* out, size_t outN);
void clockTimeApplyTz(const char* id);    // setenv TZ + tzset
