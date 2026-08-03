#include "src/faces/FaceRegistry.h"
#include "src/config/config.h"
#include "src/config/debug.h"

#include <string.h>

namespace FaceRegistry {
namespace {

static const int kMaxFaces = 16;
static FaceFactory gFactories[kMaxFaces];
static int gFactoryCount = 0;

static Face* gActive = nullptr;
static char gActiveId[32] = "";

}  // namespace

bool add(FaceFactory factory) {
  if (!factory || gFactoryCount >= kMaxFaces) {
    return false;
  }
  gFactories[gFactoryCount++] = factory;
  return true;
}

int count() {
  return gFactoryCount;
}

const FaceInfo* infoAt(int index) {
  if (index < 0 || index >= gFactoryCount) {
    return nullptr;
  }
  Face* tmp = gFactories[index]();
  if (!tmp) {
    return nullptr;
  }
  // Leak-free: factories should return statically owned OR we delete after copy
  // Convention: factory returns heap object; we only read info then delete.
  static FaceInfo infoCopy;
  infoCopy = tmp->info();
  delete tmp;
  return &infoCopy;
}

Face* createById(const char* id) {
  if (!id) {
    return nullptr;
  }
  for (int i = 0; i < gFactoryCount; i++) {
    Face* f = gFactories[i]();
    if (!f) {
      continue;
    }
    if (strcmp(f->info().id, id) == 0) {
      return f;
    }
    delete f;
  }
  return nullptr;
}

bool setActiveId(const char* id) {
  Face* next = createById(id);
  if (!next) {
    LOGE("face", "unknown id \"%s\" (registered=%d)", id ? id : "(null)", gFactoryCount);
    return false;
  }

  if (next->info().minApi > FLINT_OS_API_VERSION) {
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
