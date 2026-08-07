#pragma once

#include <stdint.h>

// Settings overlay — owns row selection, paint, and value adjusters.
// Keep this file focused: new setting rows/adjusters → helpers beside it
// (formatters, cycles), not a return to a monolithic shell god-file.

void shellSettingsEnter();
void shellSettingsNavPrev();
void shellSettingsNavNext();
void shellSettingsAdjust(int dir);
// true → open face picker (Face row activated).
bool shellSettingsApply();

void shellSettingsPaint(uint8_t fade);
void shellSettingsPaintPartial();

bool shellSettingsNeedsFull();
bool shellSettingsNeedsRows();
