#pragma once

#include "display/display.h"
#include "faces/FaceContext.h"

#include <stdint.h>

// Overlay chrome primitives — shell state machine paints via these only.

void chromeFillBackdrop(const FaceContext& ctx);

// Title + accent underline. fade 0..255 fades colors from bg toward target.
void chromeHeader(const FaceContext& ctx, const char* title, uint8_t fade = 255);

void chromeFooter(const FaceContext& ctx, const char* hint, uint8_t fade = 255);

// Row fill from rowTopY with height rowH. Selection: accent bar + selectionBg.
void chromeListRow(const FaceContext& ctx, int rowTopY, int rowH, const char* label,
                   bool selected, uint8_t fade = 255);

DisplayRect chromeListRowRect(const FaceContext& ctx, int rowTopY, int rowH);

void chromeScrollTrack(const FaceContext& ctx, int trackY, int trackH, int thumbY,
                       int thumbH);

void chromeCounter(const FaceContext& ctx, int cur, int total, uint8_t fade = 255);
