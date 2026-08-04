#include "ui/ui_dirty.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE gDirtyMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t gDirty = UI_DIRTY_FORCE;

void uiMarkDirty(uint32_t mask) {
  if (!mask) {
    return;
  }
  portENTER_CRITICAL(&gDirtyMux);
  gDirty |= mask;
  portEXIT_CRITICAL(&gDirtyMux);
}

uint32_t uiPeekDirty() {
  portENTER_CRITICAL(&gDirtyMux);
  const uint32_t v = gDirty;
  portEXIT_CRITICAL(&gDirtyMux);
  return v;
}

uint32_t uiTakeDirty() {
  portENTER_CRITICAL(&gDirtyMux);
  const uint32_t v = gDirty;
  gDirty = UI_DIRTY_NONE;
  portEXIT_CRITICAL(&gDirtyMux);
  return v;
}

bool uiHasDirty() {
  return uiPeekDirty() != UI_DIRTY_NONE;
}
