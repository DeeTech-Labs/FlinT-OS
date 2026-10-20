#include "shell/shell_picker.h"
#include "render/builders.h"
#include "render/watch_display.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "faces/FaceRegistry.h"
#include "i18n/i18n.h"
#include "settings/settings.h"
#include "ui/chrome.h"
#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

enum PickerDirty : uint8_t {
  PICK_OVL_NONE = 0,
  PICK_OVL_FULL = 1,
  PICK_OVL_ROWS = 2,
};

static int gIndex = 0;
static int gPrevIndex = 0;
static PickerDirty gDirty = PICK_OVL_FULL;

void shellPickerEnter() {
  gIndex = 0;
  const char* cur = FaceRegistry::activeId();
  for (int i = 0; i < FaceRegistry::count(); i++) {
    const FaceInfo* info = FaceRegistry::infoAt(i);
    if (info && cur && strcmp(info->id, cur) == 0) {
      gIndex = i;
      break;
    }
  }
  gPrevIndex = gIndex;
  gDirty = PICK_OVL_FULL;
}

void shellPickerNavPrev() {
  if (FaceRegistry::count() <= 0) {
    return;
  }
  gPrevIndex = gIndex;
  gIndex = (gIndex + FaceRegistry::count() - 1) % FaceRegistry::count();
  gDirty = PICK_OVL_ROWS;
}

void shellPickerNavNext() {
  if (FaceRegistry::count() <= 0) {
    return;
  }
  gPrevIndex = gIndex;
  gIndex = (gIndex + 1) % FaceRegistry::count();
  gDirty = PICK_OVL_ROWS;
}

bool shellPickerConfirm() {
  const FaceInfo* info = FaceRegistry::infoAt(gIndex);
  if (!info || !FaceRegistry::setActiveId(info->id)) {
    return false;
  }
  settingsSetActiveFace(info->id);
  return true;
}

bool shellPickerNeedsFull() {
  return gDirty == PICK_OVL_FULL;
}

bool shellPickerNeedsRows() {
  return gDirty == PICK_OVL_ROWS;
}

void shellPickerPaint(uint8_t fade) {
  (void)fade;
  auto scene=FlintRender::begin(flintTr(FlintStr::Faces));
  const int count=FaceRegistry::count();
  const int start=gIndex<4?0:gIndex-3;
  for(int i=0;i<4&&start+i<count;++i){const auto* info=FaceRegistry::infoAt(start+i);if(info)FlintRender::row(scene,i,info->name,"",start+i==gIndex);}
  FlintRender::footer(scene,"p/n · Enter · Esc");
  watchPresentScene(scene);gPrevIndex=gIndex;gDirty=PICK_OVL_NONE;
}
void shellPickerPaintPartial(){shellPickerPaint(255);}
