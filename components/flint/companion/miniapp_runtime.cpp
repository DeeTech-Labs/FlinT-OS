#include "companion/miniapps.h"
#include "util/flint_sys.h"
#include "settings/settings.h"
#include "settings/cfg_fs.h"
#include "faces/FaceRegistry.h"
#include "event/event.h"
#include "ui/ui.h"
#include <cstring>
const char* miniappActivate(const char* id) {
  if(!id)return "invalid_arguments";
  const bool base=!strcmp(id,"digital");
  const auto* package=base?nullptr:miniappFind(id);
  if(!base&&!package)return "unknown_package";
  const auto previous=settingsGet();
  if(base||package->kind!=1) {
    auto next=previous;strcpy(next.activeFace,id);
    if(!settingsApply(next))return "storage_error";
  }
  if(base||package->kind!=0) {
    if(const char* error=miniappThemeUse(base?"":id)) {
      settingsApply(previous);return error;
    }
  }
  if(base||package->kind!=1) {
    if(!FaceRegistry::setActiveId(id)) {settingsApply(previous);return "face_error";}
    uiFaceActivateCurrent();
  }
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);return nullptr;
}
const char* miniappDelete(const char* id) {
  if(!id)return "invalid_arguments";
  if(!strcmp(id,"digital"))return "protected_package";
  if(!miniappFind(id))return "unknown_package";
  char theme[25]{};cfgFsReadFile("/cfg/theme.cfg",theme,sizeof(theme));
  if(!strcmp(settingsGet().activeFace,id)||!strcmp(theme,id))
    if(const char* error=miniappActivate("digital"))return error;
  if(const char* error=miniappRemove(id))return error;
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);return nullptr;
}

uint32_t miniappFreeRam(){return flintFreeHeap();}
