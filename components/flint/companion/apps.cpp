#include "companion/apps.h"
#include "sdk/companion.h"
#include "settings/cfg_fs.h"
#include "event/event.h"
#include "esp_timer.h"
#include "usb/pc_link.h"
#include <cstring>
namespace {
CompanionState state;
bool number(const char* text,int& value,int maximum) {
  value = 0;
  if (!text || !*text) return false;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    value = value * 10 + *p - '0';
    if (value > maximum) return false;
  }
  return true;
}
int digit(char c) {
  return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1;
}
}
const CompanionState& companionState() { return state; }
uint32_t companionFocusRemaining() {
  const auto now = static_cast<uint64_t>(esp_timer_get_time()/1000);
  return state.focusDeadlineMs > now ? (state.focusDeadlineMs-now+999)/1000 : 0;
}
void companionAppsBegin() {
  if (cfgFsReadFile("/cfg/note.txt",state.note,sizeof(state.note)) < 0) state.note[0] = 0;
}
const char* companionAppCommand(const char* operation,const char* arguments) {
  if (!operation || !arguments) return "invalid_arguments";
  if (!strcmp(operation,"NOTE")) {
    const size_t length = strlen(arguments);
    if (length > 192 || length % 2) return "invalid_note";
    char note[97]{};
    for (size_t index = 0; index < length/2; ++index) {
      const int a = digit(arguments[index*2]), b = digit(arguments[index*2+1]);
      if (a < 0 || b < 0 || !(note[index] = static_cast<char>(a*16+b)) || static_cast<unsigned char>(note[index]) < 32) return "invalid_note";
    }
    if (!cfgFsWriteAtomic("/cfg/note.txt",note,length/2)) return "storage_error";
    strcpy(state.note,note); state.mode = CompanionMode::Note;
  } else if (!strcmp(operation,"FOCUS")) {
    int seconds;
    if (!number(arguments,seconds,86400)) return "invalid_duration";
    state.focusDeadlineMs = esp_timer_get_time()/1000 + static_cast<uint64_t>(seconds)*1000;
    state.mode = CompanionMode::Focus;
  } else if (!strcmp(operation,"PC")) {
    char text[16]; if (strlen(arguments) >= sizeof(text)) return "invalid_metrics";
    strcpy(text,arguments); char* separator = strchr(text,' ');
    if (!separator) return "invalid_metrics";
    *separator++ = 0;
    int cpu,memory;
    if (!number(text,cpu,100) || !number(separator,memory,100)) return "invalid_metrics";
    state.cpuPercent = cpu; state.memoryPercent = memory;
  } else if (!strcmp(operation,"SHOW")) {
    if (!strcmp(arguments,"note")) state.mode = CompanionMode::Note;
    else if (!strcmp(arguments,"focus")) state.mode = CompanionMode::Focus;
    else if (!strcmp(arguments,"computer")) state.mode = CompanionMode::Computer;
    else return "invalid_mode";
  } else if (!strcmp(operation,"FIND")) {
    if (*arguments) return "invalid_arguments";
    if (!pcLinkFind()) return "unsupported_hardware";
  } else return "unsupported_operation";
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
  return nullptr;
}
