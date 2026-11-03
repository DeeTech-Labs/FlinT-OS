#include "faces/FaceRegistry.h"
#include "config/config.h"
#include "companion/miniapps.h"

Face* createMiniappFace(const MiniappDefinition& definition);
#include "config/debug.h"

#include <string.h>

namespace FaceRegistry {
namespace {

static const int kMaxFaces = 1;

struct FaceEntry {
  FaceFactory factory;
  FaceInfo info;
};

static FaceEntry gEntries[kMaxFaces];
static int gFactoryCount = 0;

static Face* gActive = nullptr;
static char gActiveId[32] = "";

bool apiOk(const FaceInfo& info) {
  return info.minApi <= FLINT_OS_API_VERSION;
}

}  // namespace

bool add(FaceFactory factory, const FaceInfo& info) {
  if (!factory || gFactoryCount >= kMaxFaces) {
    return false;
  }
  if (!info.id || !info.id[0]) {
    return false;
  }
  if (!apiOk(info)) {
    LOGE("face", "reject \"%s\": needs API %u, have %u",
         info.id, (unsigned)info.minApi, (unsigned)FLINT_OS_API_VERSION);
    return false;
  }

  gEntries[gFactoryCount].factory = factory;
  gEntries[gFactoryCount].info = info;
  gFactoryCount++;
  return true;
}

int count() {
  int count = gFactoryCount;
  for (int i=0;i<miniappsCount();++i) if (miniappAt(i)->kind != 1) ++count;
  return count;
}

const FaceInfo* infoAt(int index) {
  if (index < 0) return nullptr;
  if (index < gFactoryCount) return &gEntries[index].info;
  index -= gFactoryCount;
  static FaceInfo metadata[MiniappSlots];
  for (int i=0;i<miniappsCount();++i) {
    const auto* p = miniappAt(i);
    if (p->kind == 1) continue;
    if (index-- == 0) {
      metadata[i] = {p->id,p->name,"Miniapp",p->version,1};
      return &metadata[i];
    }
  }
  return nullptr;
}

Face* createById(const char* id) {
  if (const auto* p = miniappFind(id)) return p->kind != 1 ? createMiniappFace(*p) : nullptr;
  if (!id) {
    return nullptr;
  }
  for (int i = 0; i < gFactoryCount; i++) {
    if (strcmp(gEntries[i].info.id, id) == 0) {
      return gEntries[i].factory();
    }
  }
  return nullptr;
}

bool setActiveId(const char* id) {
  Face* next = createById(id);
  if (!next) {
    LOGE("face", "unknown id \"%s\" (registered=%d)", id ? id : "(null)", gFactoryCount);
    return false;
  }

  if (!apiOk(next->info())) {
    LOGE("face", "\"%s\" needs API %u, FlinT OS has %u",
         id, (unsigned)next->info().minApi, (unsigned)FLINT_OS_API_VERSION);
    delete next;
    return false;
  }

  if (gActive) {
    gActive->onDeactivate();
    delete gActive;
    gActive = nullptr;
  }

  gActive = next;
  strncpy(gActiveId, id, sizeof(gActiveId) - 1);
  gActiveId[sizeof(gActiveId) - 1] = '\0';
  LOGI("face", "active=\"%s\" (%s) v%u",
       gActive->info().id, gActive->info().name, (unsigned)gActive->info().version);
  return true;
}

const char* activeId() {
  return gActiveId;
}

Face* active() {
  return gActive;
}

}  // namespace FaceRegistry
