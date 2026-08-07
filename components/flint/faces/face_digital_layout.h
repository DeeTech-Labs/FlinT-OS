#pragma once

#include "display/display.h"
#include "faces/FaceContext.h"

#include <stdint.h>

// Layout metrics + dirty-band helpers for the built-in digital face.

struct DigitalTimeLayout {
  int digitW = 0;
  int colonW = 0;
  int glyphWDigit[10] = {0};
  int glyphWDash = 0;
  int cellX[8] = {0};
  int endX = 0;
  int glyphH = 0;
  int ascent = 0;
  uint8_t len = 8;
  bool ready = false;
  bool withSeconds = true;

  void reset() { ready = false; }

  void ensure(const FaceContext& ctx, bool withSeconds, bool hour12,
              const lgfx::IFont* timeFont);

  DisplayRect timeBand(int screenH) const;
  DisplayRect ampmBand(int screenH) const;
  int appendColonRects(int screenH, const char* timeStr, DisplayRect* out, int cap) const;

  void drawGlyph(lgfx::LGFXBase& gfx, int index, char ch, uint16_t fg, uint16_t bg,
                 int screenH, const lgfx::IFont* timeFont) const;
};

DisplayRect digitalDateBand(const FaceContext& ctx, const char* dateStr, int* ioPresentW,
                            const lgfx::IFont* bodyFont);

uint8_t digitalColonFadeNow();
uint16_t digitalColonColor(const UiPalette& pal, uint8_t fade);
bool digitalColonFadeChanged(uint8_t fade, uint8_t lastFade);
