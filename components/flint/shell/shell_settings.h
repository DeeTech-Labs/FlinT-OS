#pragma once

#include <stdint.h>

// Settings overlay — owns row selection, paint, and value adjusters.
// Do not grow this file with more rows/cycles inline: extract formatters /
// cycle helpers (or paint) beside it. New settings → helper first, not god-file.

void shellSettingsEnter();
void shellSettingsNavPrev();
void shellSettingsNavNext();
void shellSettingsAdjust(int dir);
// true → open face picker (Face row activated).
bool shellSettingsApply();
// true → leave settings; false → returned to the category list.
bool shellSettingsBack();

void shellSettingsPaint(uint8_t fade);
void shellSettingsPaintPartial();

bool shellSettingsNeedsFull();
bool shellSettingsNeedsRows();
