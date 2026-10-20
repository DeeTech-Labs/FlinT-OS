#include "companion/control.h"
#include "companion/action_input.h"
#include "settings/settings.h"
#include "faces/FaceRegistry.h"
#include "time/tz_db.h"
#include "event/event.h"
#include "host_check.h"
#include "sdk/companion.h"
#include <cstring>
#include <string>
static FlintSettings settings{"digital",255,0,0,5,0,"Europe/Moscow"};
static bool storageOkay = true;
static unsigned writes = 0;
const FlintSettings& settingsGet() { return settings; }
bool settingsApply(const FlintSettings& next) { if (!storageOkay) return false; settings = next; ++writes; return true; }
const CompanionState& companionState() { static CompanionState state; return state; }
uint32_t companionFocusRemaining() { return 0; }
const char* companionAppCommand(const char*,const char*) { return "unsupported_operation"; }
bool studioInstall(const char*) { return false; }
void uiFaceActivateCurrent() {}
void flintEventEmit(FlintEvent) {}
int tzDbIndexOf(const char* id) { return !strcmp(id,"UTC") ? 0 : !strcmp(id,"Europe/Moscow") ? 1 : -1; }
int tzDbCount() { return 2; }
const TzEntry* tzDbAt(int index) { static TzEntry zones[]{{"UTC","UTC","UTC"},{"Europe/Moscow","MSK-3","Moscow"}}; return &zones[index]; }
namespace FaceRegistry {
int count() { return 1; }
const FaceInfo* infoAt(int) { static FaceInfo info{"digital","Digital","DeeTech",1,1}; return &info; }
bool setActiveId(const char* id) { return !strcmp(id,"digital"); }
}
static std::string exchange(const char* operation, const char* payload = "") {
  PcCommand command; command.kind = PcCommandKind::Request; command.requestId = 42;
  strcpy(command.operation,operation); strcpy(command.payload,payload);
  char response[896]; companionExecute(command,response,sizeof(response),true); return response;
}
int main() {
  hostRequire(flintActionInput.push("primary"), "emulate input");
  hostRequire(!flintActionInput.push("shell command"), "reject unknown buttons");
  hostRequire(exchange("EVENTS", "0").find("primary") != std::string::npos, "route event to client cursor");
  hostRequire(exchange("EVENTS", "1").find("\"events\":[]") != std::string::npos, "no replay after cursor");
  hostRequire(exchange("EVENTS", "999999999999999999999").find("invalid_cursor") != std::string::npos, "reject cursor overflow");
  for (int i = 0; i < 10; ++i) flintActionInput.push("secondary");
  hostRequire(exchange("EVENTS", "0").find("\"lost\":true") != std::string::npos, "report bounded queue loss");
  PcCommand untrusted; untrusted.kind = PcCommandKind::Request; untrusted.requestId = 3; strcpy(untrusted.operation,"APPS");
  char denied[896]; companionExecute(untrusted,denied,sizeof(denied));
  hostRequire(std::string(denied).find("authenticated_transport_required") != std::string::npos,"BLE cannot read private notes without authenticated pairing");
  hostRequire(exchange("SETTINGS").find("\"id\":42,\"ok\":true") != std::string::npos, "correlated settings response");
  hostRequire(exchange("SET","show_seconds=1 timezone=UTC").find("\"ok\":true") != std::string::npos, "persist valid patch");
  hostRequire(settings.showSeconds == 1 && !strcmp(settings.tzId,"UTC") && writes == 1, "one atomic settings write");
  for (const char* invalid : {"show_seconds=255 language=9", "language=1 timezone=Bad/Zone", "brightness=100", "language=0 language=1", "hour_format=-1", "date_format=6", "show_seconds=2", "", "language=1x"}) {
    hostRequire(exchange("SET",invalid).find("\"ok\":false") != std::string::npos, "invalid patch rejected");
    hostRequire(settings.showSeconds == 1 && settings.lang == 0 && writes == 1, "invalid patch never partially applies");
  }
  storageOkay = false;
  hostRequire(exchange("SET","language=1").find("storage_error") != std::string::npos && settings.lang == 0, "failed persistence is not acknowledged");
  storageOkay = true;
  hostRequire(exchange("FACES").find("digital") != std::string::npos, "enumerate firmware faces");
  hostRequire(exchange("FACE","unknown").find("unknown_face") != std::string::npos, "reject missing face");
  hostRequire(exchange("TIMEZONES").find("Europe/Moscow") != std::string::npos, "enumerate supported zones");
  hostRequire(exchange("UNKNOWN").find("unsupported_operation") != std::string::npos, "structured unknown operation");
}

const char* bleLinkState() { return "disabled"; }
int bleLinkLastDisconnectReason() { return 0; }
int bleLinkLastEncryptionStatus() { return -1; }

bool watchDisplayCommand(const char*,const char*,char*,size_t) { return false; }
