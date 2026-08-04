#include "time/tz_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config/debug.h"

static const TzEntry kZones[] = {
    {"Europe/Moscow", "MSK-3", "Moscow"},
    {"Europe/Kaliningrad", "EET-2", "Kaliningrad"},
    {"Europe/Samara", "SAMT-4", "Samara"},
    {"Asia/Yekaterinburg", "YEKT-5", "Yekaterinburg"},
    {"Asia/Omsk", "OMST-6", "Omsk"},
    {"Asia/Krasnoyarsk", "KRAT-7", "Krasnoyarsk"},
    {"Asia/Irkutsk", "IRKT-8", "Irkutsk"},
    {"Asia/Yakutsk", "YAKT-9", "Yakutsk"},
    {"Asia/Vladivostok", "VLAT-10", "Vladivostok"},
    {"Asia/Magadan", "MAGT-11", "Magadan"},
    {"Asia/Kamchatka", "PETT-12", "Kamchatka"},
    {"UTC", "UTC0", "UTC"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0", "London"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3", "Berlin"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3", "Paris"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Helsinki"},
    {"Europe/Istanbul", "TRT-3", "Istanbul"},
    {"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Kyiv"},
    {"Asia/Dubai", "GST-4", "Dubai"},
    {"Asia/Kolkata", "IST-5:30", "Kolkata"},
    {"Asia/Bangkok", "ICT-7", "Bangkok"},
    {"Asia/Shanghai", "CST-8", "Shanghai"},
    {"Asia/Tokyo", "JST-9", "Tokyo"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3", "Sydney"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0", "New York"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0", "Chicago"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0", "Denver"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0", "Los Angeles"},
    {"America/Sao_Paulo", "BRT3", "Sao Paulo"},
};

int tzDbCount() {
  return static_cast<int>(sizeof(kZones) / sizeof(kZones[0]));
}

const TzEntry* tzDbAt(int index) {
  if (index < 0 || index >= tzDbCount()) {
    return nullptr;
  }
  return &kZones[index];
}

const TzEntry* tzDbFind(const char* id) {
  if (id && id[0]) {
    for (int i = 0; i < tzDbCount(); i++) {
      if (strcmp(kZones[i].id, id) == 0) {
        return &kZones[i];
      }
    }
  }
  return &kZones[0];  // Europe/Moscow
}

int tzDbIndexOf(const char* id) {
  if (!id) {
    return -1;
  }
  for (int i = 0; i < tzDbCount(); i++) {
    if (strcmp(kZones[i].id, id) == 0) {
      return i;
    }
  }
  return -1;
}

void tzDbUrlEncodeId(const char* id, char* out, size_t outN) {
  if (!out || outN == 0) {
    return;
  }
  out[0] = '\0';
  if (!id) {
    return;
  }
  size_t o = 0;
  for (const char* p = id; *p && o + 4 < outN; p++) {
    if (*p == '/') {
      out[o++] = '%';
      out[o++] = '2';
      out[o++] = 'F';
    } else {
      out[o++] = *p;
    }
  }
  out[o] = '\0';
}

void clockTimeApplyTz(const char* id) {
  const TzEntry* e = tzDbFind(id);
  setenv("TZ", e->posix, 1);
  tzset();
  LOGI("tz", "apply id=%s posix=%s", e->id, e->posix);
}
