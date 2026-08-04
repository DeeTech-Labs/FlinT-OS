#include "faces/FaceRegistry.h"
#include "config/config.h"
#include "config/debug.h"

#include <string.h>

namespace FaceRegistry {
namespace {

static const int kMaxFaces = 16;
static FaceFactory gFactories[kMaxFaces];
static int gFactoryCount = 0;

static Face* gActive = nullptr;
static char gActiveId[32] = "";

bool apiOk(const FaceInfo& info) {
  return info.minApi <= FLINT_OS_API_VERSION;
}

}  // namespace

bool add(FaceFactory factory) {
  if (!factory || gFactoryCount >= kMaxFaces) {
    return false;
  }

  Face* probe = factory();
  if (!probe) {
    return false;
  }
  const FaceInfo& info = probe->info();
  if (!apiOk(info)) {
    LOGE("face", "reject \"%s\": needs API %u, have %u",
         info.id ? info.id : "?", (unsigned)info.minApi, (unsigned)FLINT_OS_API_VERSION);
    delete probe;
    return false;
  }
  delete probe;

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
