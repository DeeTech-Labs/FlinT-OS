#pragma once

#include "LgfxBoard.h"
#include "fonts_u8g2.h"

#include <stdint.h>

extern LGFX tft;
extern int SCR_W;
extern int SCR_H;

struct DisplayRect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

void displayBegin();

// Off-screen canvas (sprite) when RAM allows.
lgfx::LGFXBase& displayGfx();
bool displayHasCanvas();
void displayPresent();  // push canvas → panel (no-op without canvas)
void displayPresentRect(int x, int y, int w, int h);
void displayPresentRects(const DisplayRect* rects, int count);

// Clip drawing on the active canvas (sprite or panel). Use around partial paints
// so glyphs cannot smear outside dirty bands; always clear when done.
void displaySetClipRect(int x, int y, int w, int h);
void displayClearClipRect();

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t colorLerp(uint16_t a, uint16_t b, uint8_t t /*0..255*/);

void displayShowStatus(const char* msg);
void displayClearStatus();

void displaySetFont(lgfx::LGFXBase& gfx, const lgfx::IFont* font);
int displayTextWidth(lgfx::LGFXBase& gfx, const char* text);
int displayFontAscent(lgfx::LGFXBase& gfx);
int displayFontDescent(lgfx::LGFXBase& gfx);
void displayDrawText(lgfx::LGFXBase& gfx, const char* text, int x, int yBaseline,
                     uint16_t fg, uint16_t bg, const lgfx::IFont* font);
void displayDrawUtf8Centered(const char* text, int yBaseline, uint16_t fg,
                             const lgfx::IFont* font);
