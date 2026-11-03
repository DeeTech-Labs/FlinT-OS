#pragma once

#include <stdint.h>

// Face picker overlay — owns selection + dirty; shell drives enter/nav/paint.

void shellPickerEnter();
void shellPickerNavPrev();
void shellPickerNavNext();
// Activate selected face into settings + registry. true if applied.
bool shellPickerConfirm();

void shellPickerPaint(uint8_t fade);
void shellPickerPaintPartial();

bool shellPickerNeedsFull();
bool shellPickerNeedsRows();

void shellPickerDelete();
bool shellPickerBack(); // false cancels the delete dialog, true exits the picker
