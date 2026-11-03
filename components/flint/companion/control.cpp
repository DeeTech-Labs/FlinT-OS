#include "companion/control.h"
#include "render/watch_display.h"
#include "companion/action_input.h"
#include "ble/ble_link.h"
#include "companion/ignits.h"
#include "usb/pc_data.h"
#include "settings/settings.h"
#include "storage/package_storage.h"
#include "faces/FaceRegistry.h"
#include "time/tz_db.h"
#include "event/event.h"
#include "ui/ui.h"
#include "time/clock_time.h"
#include <cstring>
#include <cstdlib>

namespace {
void result(const PcCommand& command, char* response, size_t capacity, const char* json, const char* error = nullptr) {
  char body[864];
  PcJsonWriter writer(body, sizeof(body));
  writer.raw("{\"id\":"); writer.number(command.requestId);
  writer.raw(",\"ok\":"); writer.boolean(error == nullptr);
  if (error) { writer.raw(",\"error\":"); writer.text(error); }
  else { writer.raw(",\"result\":"); writer.raw(json); }
  writer.raw("}");
  if (writer.finish()) snprintf(response, capacity, "FLINT RESULT %s\n", body);
  else snprintf(response, capacity, "FLINT RESULT {\"id\":%lu,\"ok\":false,\"error\":\"overflow\"}\n", static_cast<unsigned long>(command.requestId));
}
bool integer(const char* value, uint8_t& destination) {
  if (!*value) return false;
  unsigned n = 0;
  for (const char* p = value; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + *p - '0';
    if (n > 255) return false;
  }
  destination = n;
  return true;
}
void settingsJson(char* json, size_t capacity) {
  const auto& settings = settingsGet();
  PcJsonWriter writer(json, capacity);
  writer.raw("{\"show_seconds\":"); writer.number(settings.showSeconds);
  writer.raw(",\"language\":"); writer.number(settings.lang);
  writer.raw(",\"hour_format\":"); writer.number(settings.hourFormat);
  writer.raw(",\"date_format\":"); writer.number(settings.dateFormat);
  writer.raw(",\"date_separator\":"); writer.number(settings.dateSep);
  writer.raw(",\"timezone\":"); writer.text(settings.tzId);
  writer.raw(",\"active_face\":"); writer.text(settings.activeFace);
  writer.raw("}"); writer.finish();
}
}
void companionExecute(const PcCommand& command, char* response, size_t capacity, bool trustedTransport) {
  char json[672]{};
  if (!trustedTransport && strcmp(command.operation,"SETTINGS") && strcmp(command.operation,"SET")
      && strcmp(command.operation,"FACES") && strcmp(command.operation,"FACE") && strcmp(command.operation,"TIMEZONES")) {
    result(command,response,capacity,nullptr,"authenticated_transport_required"); return;
  }
  if (!strcmp(command.operation,"SCREEN") || !strcmp(command.operation,"SCENE") || !strcmp(command.operation,"SHOT") || !strcmp(command.operation,"PIXELS")) {
    if (!watchDisplayCommand(command.operation,command.payload,json,sizeof(json))) {result(command,response,capacity,nullptr,"screen_changed_or_unavailable");return;}
  } else if (!strcmp(command.operation, "EVENTS")) {
    uint32_t after = 0;
    if (!command.payload[0]) after = flintActionInput.sequence();
    else {
      for (const char* p = command.payload; *p; ++p) {
        if (*p < '0' || *p > '9' || after > (UINT32_MAX - unsigned(*p - '0')) / 10) {
          result(command,response,capacity,nullptr,"invalid_cursor"); return;
        }
        after = after * 10 + *p - '0';
      }
    }
    if (!flintActionInput.json(after, json, sizeof(json))) { result(command,response,capacity,nullptr,"invalid_cursor"); return; }
  } else if (!strcmp(command.operation,"BLE")) {
    PcJsonWriter writer(json,sizeof(json)); writer.raw("{\"state\":"); writer.text(bleLinkState());
    writer.raw(",\"disconnect_reason\":"); writer.number(bleLinkLastDisconnectReason());
    writer.raw(",\"encryption_status\":"); writer.number(bleLinkLastEncryptionStatus()); writer.raw("}"); writer.finish();
  } else if (!strcmp(command.operation, "SETTINGS")) {
    if (command.payload[0]) { result(command, response, capacity, nullptr, "invalid_arguments"); return; }
    settingsJson(json, sizeof(json));
  } else if (!strcmp(command.operation, "SET")) {
    FlintSettings next = settingsGet();
    char payload[sizeof(command.payload)];
    strcpy(payload, command.payload);
    char* save = nullptr;
    unsigned mask = 0;
    for (char* entry = strtok_r(payload, " ", &save); entry; entry = strtok_r(nullptr, " ", &save)) {
      char* value = strchr(entry, '=');
      if (!value) { result(command, response, capacity, nullptr, "invalid_arguments"); return; }
      *value++ = 0;
      unsigned bit = 0;
      bool valid = true;
      if (!strcmp(entry, "show_seconds")) { bit = 1; valid = integer(value, next.showSeconds) && (next.showSeconds == 0 || next.showSeconds == 1 || next.showSeconds == 255); }
      else if (!strcmp(entry, "language")) { bit = 2; valid = integer(value, next.lang) && next.lang <= 1; }
      else if (!strcmp(entry, "hour_format")) { bit = 4; valid = integer(value, next.hourFormat) && next.hourFormat <= 1; }
      else if (!strcmp(entry, "date_format")) { bit = 8; valid = integer(value, next.dateFormat) && next.dateFormat < SETTINGS_DATE_FORMAT_COUNT; }
      else if (!strcmp(entry, "date_separator")) { bit = 16; valid = integer(value, next.dateSep) && next.dateSep < SETTINGS_DATE_SEP_COUNT; }
      else if (!strcmp(entry, "timezone")) {
        bit = 32; valid = strlen(value) < sizeof(next.tzId) && tzDbIndexOf(value) >= 0;
        if (valid) strcpy(next.tzId, value);
      } else valid = false;
      if (!valid || (mask & bit)) { result(command, response, capacity, nullptr, "invalid_setting"); return; }
      mask |= bit;
    }
    if (!mask) { result(command, response, capacity, nullptr, "invalid_arguments"); return; }
    if (!settingsApply(next)) { result(command, response, capacity, nullptr, "storage_error"); return; }
    flintEventEmit(FLINT_EVT_SETTINGS);
    settingsJson(json, sizeof(json));
  } else if (!strcmp(command.operation, "IGNITINSTALL")) {
    const char* error = ignitInstall(command.payload);
    if (error) { result(command,response,capacity,nullptr,error); return; }
    ignitThemeRestore();
    FaceRegistry::setActiveId(settingsGet().activeFace);
    uiFaceActivateCurrent();
    flintEventEmit(FLINT_EVT_FORCE_REDRAW);
    strcpy(json,"{\"installed\":true}");
  } else if (!strcmp(command.operation, "IGNITREMOVE")) {
    const char* error = ignitDelete(command.payload);
    if (error) { result(command,response,capacity,nullptr,error); return; }
    strcpy(json,"{\"removed\":true}");
  } else if (!strcmp(command.operation, "IGNITTHEME")) {
    const char* error=ignitThemeUse(command.payload);
    if(error) { result(command,response,capacity,nullptr,error);return; }
    flintEventEmit(FLINT_EVT_FORCE_REDRAW);
    strcpy(json,"{\"applied\":true}");
  } else if (!strcmp(command.operation, "IGNITUSE")) {
    const char* error=ignitActivate(command.payload);
    if(error){result(command,response,capacity,nullptr,error);return;}
    strcpy(json,"{\"applied\":true}");
  } else if (!strcmp(command.operation, "IGNITSTORAGE")) {
    PackageStorageInfo storage;
    if(!packageStorageInfo(storage)){result(command,response,capacity,nullptr,"storage_unavailable");return;}
    snprintf(json,sizeof(json),"{\"partition\":\"storage\",\"total\":%u,\"used\":%u,\"free\":%u,\"reserved\":%u,\"write_budget\":%u,\"capacity\":%d,\"installed\":%d,\"free_ram\":%u,\"ram_reserve\":%u}",
        unsigned(storage.total),unsigned(storage.used),unsigned(storage.free),unsigned(IgnitStorageReserve),unsigned(IgnitAtomicWriteBudget),IgnitSlots,ignitsCount(),unsigned(ignitFreeRam()),unsigned(IgnitRamReserve));
  } else if (!strcmp(command.operation, "IGNITS")) {
    PcJsonWriter writer(json,sizeof(json));writer.raw("[{\"id\":\"digital\",\"name\":\"Spark Base\",\"version\":3,\"kind\":2,\"protected\":true}");
    for(int i=0;i<ignitsCount();++i) {
      const auto* p=ignitAt(i);writer.raw(",");
      writer.raw("{\"id\":");writer.text(p->id);
      writer.raw(",\"name\":");writer.text(p->name);
      writer.raw(",\"version\":");writer.number(p->version);
      writer.raw(",\"kind\":");writer.number(p->kind);writer.raw("}");
    }
    writer.raw("]");if(!writer.finish()){result(command,response,capacity,nullptr,"overflow");return;}
  } else if (!strcmp(command.operation, "FACES")) {
    PcJsonWriter writer(json, sizeof(json));
    writer.raw("[");
    for (int index = 0; index < FaceRegistry::count(); ++index) {
      const auto* info = FaceRegistry::infoAt(index);
      if (index) writer.raw(",");
      writer.raw("{\"id\":"); writer.text(info->id);
      writer.raw(",\"name\":"); writer.text(info->name);
      writer.raw(",\"version\":"); writer.number(info->version);
      writer.raw("}");
    }
    writer.raw("]");
    if (!writer.finish()) { result(command, response, capacity, nullptr, "overflow"); return; }
  } else if (!strcmp(command.operation, "FACE")) {
    const char* error=ignitActivate(command.payload);
    if(error){result(command,response,capacity,nullptr,error);return;}
    settingsJson(json,sizeof(json));
  } else if (!strcmp(command.operation, "TIMEZONES")) {
    PcJsonWriter writer(json, sizeof(json)); writer.raw("[");
    for (int i = 0; i < tzDbCount(); ++i) { if (i) writer.raw(","); writer.text(tzDbAt(i)->id); }
    writer.raw("]");
    if (!writer.finish()) { result(command, response, capacity, nullptr, "overflow"); return; }
  } else {
    result(command,response,capacity,nullptr,"unsupported_operation");return;
  }
  result(command, response, capacity, json);
}
