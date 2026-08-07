#include "shell/shell_picker.h"

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

static void paintRow(const FaceContext& ctx, int index, uint8_t fade) {
  const FaceInfo* info = FaceRegistry::infoAt(index);
  if (!info) {
    return;
  }
  char line[64];
  snprintf(line, sizeof(line), "%.20s  (%.16s)", info->name ? info->name : "?",
           info->id ? info->id : "?");
  chromeListRow(ctx, UiTok::shellRowFillY(index), UiTok::kShellRowH, line, index == gIndex,
                fade);
}

void shellPickerPaint(uint8_t fade) {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  chromeFillBackdrop(ctx);
  chromeHeader(ctx, flintTr(FlintStr::Faces), fade);
  chromeFooter(ctx, flintTr(FlintStr::PNEnterEsc), fade);
  const int n = FaceRegistry::count();
  for (int i = 0; i < n; i++) {
    paintRow(ctx, i, fade);
  }
  displayPresent();
  gPrevIndex = gIndex;
  gDirty = PICK_OVL_NONE;
}

void shellPickerPaintPartial() {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  DisplayRect rects[2];
  int n = 0;
  auto add = [&](int index) {
    if (index < 0 || index >= FaceRegistry::count()) {
      return;
    }
    const DisplayRect band =
        chromeListRowRect(ctx, UiTok::shellRowFillY(index), UiTok::kShellRowH);
    displaySetClipRect(band.x, band.y, band.w, band.h);
    paintRow(ctx, index, 255);
    rects[n++] = band;
  };
  if (gPrevIndex != gIndex) {
    add(gPrevIndex);
  }
  add(gIndex);
  displayClearClipRect();
  displayPresentRects(rects, n);
  gPrevIndex = gIndex;
  gDirty = PICK_OVL_NONE;
}
