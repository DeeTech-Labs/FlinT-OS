#include "companion/apps.h"
#include "sdk/companion.h"
#include "event/event.h"
#include "host_check.h"
#include <string>
#include <cstring>
static int64_t now = 1000000;
static bool storageOkay = true;
static std::string saved;
int64_t esp_timer_get_time() { return now; }
bool pcLinkFind() { return true; }
void flintEventEmit(FlintEvent) {}
bool cfgFsWriteAtomic(const char*,const void* data,size_t length) { if (!storageOkay) return false; saved.assign(static_cast<const char*>(data),length); return true; }
int cfgFsReadFile(const char*,char* buffer,size_t size) { if (saved.empty() || saved.size()+1 > size) return -1; strcpy(buffer,saved.c_str()); return saved.size(); }
int main() {
  hostRequire(!companionAppCommand("NOTE","48656c6c6f"),"save note");
  hostRequire(std::string(companionState().note) == "Hello","decode note");
  storageOkay = false;
  hostRequire(companionAppCommand("NOTE","41") && std::string(companionState().note) == "Hello","failed save preserves note");
  storageOkay = true;
  for (const char* text : {"0", "zz", "00", "0a"}) hostRequire(companionAppCommand("NOTE",text) != nullptr,"reject malformed/control text");
  hostRequire(!companionAppCommand("FOCUS","60") && companionFocusRemaining() == 60,"start focus");
  now += 1500000;
  hostRequire(companionFocusRemaining() == 59,"focus independent of wall clock");
  hostRequire(companionAppCommand("FOCUS","9999999999999999999999") != nullptr,"bounded duration");
  hostRequire(!companionAppCommand("PC","42 71") && companionState().cpuPercent == 42,"PC metrics");
  hostRequire(companionAppCommand("PC","101 71") != nullptr && companionState().cpuPercent == 42,"reject invalid metrics");
  hostRequire(!companionAppCommand("SHOW","computer") && companionState().mode == CompanionMode::Computer,"select companion screen");
  hostRequire(!companionAppCommand("FIND",""),"find board");
  hostRequire(!companionAppCommand("FOCUS","0") && companionFocusRemaining() == 0,"stop focus");
}
