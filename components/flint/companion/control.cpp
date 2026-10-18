#include "companion/control.h"
#include "ble/ble_link.h"
#include "companion/studio.h"
#include "companion/apps.h"
#include "sdk/companion.h"
#include "usb/pc_data.h"
#include "settings/settings.h"
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
      && strcmp(command.operation,"FACES") && strcmp(command.operation,"FACE") && strcmp(command.operation,"TIMEZONES")
      && strcmp(command.operation,"STUDIO")) {
    result(command,response,capacity,nullptr,"authenticated_transport_required"); return;
  }
  if (!strcmp(command.operation,"BLE")) {
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
  } else if (!strcmp(command.operation, "APPS")) {
    const auto& state = companionState();
    PcJsonWriter writer(json,sizeof(json));
    writer.raw("{\"note\":"); writer.text(state.note);
    writer.raw(",\"focus_remaining\":"); writer.number(companionFocusRemaining());
    writer.raw(",\"cpu\":"); writer.number(state.cpuPercent);
    writer.raw(",\"memory\":"); writer.number(state.memoryPercent);
    writer.raw("}");
    if (!writer.finish()) { result(command,response,capacity,nullptr,"overflow"); return; }
  } else if (!strcmp(command.operation, "STUDIO")) {
    if (!studioInstall(command.payload)) { result(command, response, capacity, nullptr, "invalid_definition_or_storage"); return; }
    flintEventEmit(FLINT_EVT_FORCE_REDRAW);
    strcpy(json,"{\"installed\":true}");
  } else if (!strcmp(command.operation, "FACES")) {
    PcJsonWriter writer(json, sizeof(json));
    writer.raw("[");
    for (int index = 0; index < FaceRegistry::count(); ++index) {
      const auto* info = FaceRegistry::infoAt(index);
      if (index) writer.raw(",");
      writer.raw("{\"id\":"); writer.text(info->id);
      writer.raw(",\"name\":"); writer.text(info->name);
      writer.raw(",\"author\":"); writer.text(info->author);
      writer.raw(",\"version\":"); writer.number(info->version);
      writer.raw(",\"min_api\":"); writer.number(info->minApi);
      writer.raw("}");
    }
    writer.raw("]");
    if (!writer.finish()) { result(command, response, capacity, nullptr, "overflow"); return; }
  } else if (!strcmp(command.operation, "FACE")) {
    bool exists = false;
    for (int i = 0; i < FaceRegistry::count(); ++i) exists |= !strcmp(FaceRegistry::infoAt(i)->id, command.payload);
    if (!exists) { result(command, response, capacity, nullptr, "unknown_face"); return; }
    auto next = settingsGet();
    const auto previous = next;
    if (strlen(command.payload) >= sizeof(next.activeFace)) { result(command, response, capacity, nullptr, "invalid_arguments"); return; }
    strcpy(next.activeFace, command.payload);
    if (!settingsApply(next)) { result(command, response, capacity, nullptr, "storage_error"); return; }
    if (!FaceRegistry::setActiveId(command.payload)) {
      settingsApply(previous);
      result(command, response, capacity, nullptr, "face_error"); return;
    }
    uiFaceActivateCurrent();
    flintEventEmit(FLINT_EVT_FORCE_REDRAW);
    settingsJson(json, sizeof(json));
  } else if (!strcmp(command.operation, "TIMEZONES")) {
    PcJsonWriter writer(json, sizeof(json)); writer.raw("[");
    for (int i = 0; i < tzDbCount(); ++i) { if (i) writer.raw(","); writer.text(tzDbAt(i)->id); }
    writer.raw("]");
    if (!writer.finish()) { result(command, response, capacity, nullptr, "overflow"); return; }
  } else {
    const char* error = companionAppCommand(command.operation,command.payload);
    if (error) { result(command, response, capacity, nullptr, error); return; }
    strcpy(json,"{\"accepted\":true}");
  }
  result(command, response, capacity, json);
}
