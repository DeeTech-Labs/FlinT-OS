#include "display.h"
#include "src/config/config.h"
#include "src/config/debug.h"

LGFX tft;
static LGFX_Sprite gCanvas(&tft);
static bool gHasCanvas = false;

int SCR_W = 320;
int SCR_H = 240;

void displayBegin() {
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.setColorDepth(16);
  SCR_W = tft.width();
  SCR_H = tft.height();
  tft.setTextWrap(false);

  gCanvas.setColorDepth(16);
  // Allocate before Wi‑Fi so heap is larger. Full RGB565 frame ≈ 150 KB.
  if (gCanvas.createSprite(SCR_W, SCR_H)) {
    gHasCanvas = true;
    gCanvas.setTextWrap(false);
    LOGI("disp", "ST7789 %dx%d canvas OK heap=%u", SCR_W, SCR_H,
         (unsigned)ESP.getFreeHeap());
  } else {
    gHasCanvas = false;
    LOGI("disp", "ST7789 %dx%d canvas OOM — direct draw heap=%u", SCR_W, SCR_H,
         (unsigned)ESP.getFreeHeap());
  }
}

lgfx::LGFXBase& displayGfx() {
  if (gHasCanvas) {
    return gCanvas;
  }
  return tft;
}

bool displayHasCanvas() {
  return gHasCanvas;
}

void displayPresent() {
  if (!gHasCanvas) {
    return;
  }
  // Non-DMA push avoids rare DMA wait stalls while Wi‑Fi is active on C3.
  gCanvas.pushSprite(&tft, 0, 0);
}

void displayPresentRect(int x, int y, int w, int h) {
  if (!gHasCanvas) {
    return;
  }
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  if (w <= 0 || h <= 0) return;

  const uint16_t* buf = static_cast<const uint16_t*>(gCanvas.getBuffer());
  if (!buf) {
    displayPresent();
    return;
  }

  tft.startWrite();
  for (int row = 0; row < h; row++) {
    // LovyanGFX: setAddrWindow(x, y, w, h)
    tft.setAddrWindow(x, y + row, w, 1);
    tft.writePixels(buf + (size_t)(y + row) * (size_t)SCR_W + x, w);
  }
  tft.endWrite();
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

uint16_t colorLerp(uint16_t c0, uint16_t c1, uint8_t t) {
  uint8_t r0 = (c0 >> 11) << 3;
  uint8_t g0 = ((c0 >> 5) & 0x3F) << 2;
  uint8_t b0 = (c0 & 0x1F) << 3;
  uint8_t r1 = (c1 >> 11) << 3;
  uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
  uint8_t b1 = (c1 & 0x1F) << 3;
  uint8_t r = r0 + ((int)(r1 - r0) * t) / 255;
  uint8_t g = g0 + ((int)(g1 - g0) * t) / 255;
  uint8_t b = b0 + ((int)(b1 - b0) * t) / 255;
  return rgb565(r, g, b);
}

void displayFillGradientV(uint16_t top, uint16_t bottom) {
  auto& gfx = displayGfx();
  for (int y = 0; y < SCR_H; y++) {
    uint8_t t = (uint8_t)((y * 255) / (SCR_H - 1));
    gfx.drawFastHLine(0, y, SCR_W, colorLerp(top, bottom, t));
  }
  displayPresent();
}

void displayShowStatus(const char* msg) {
  auto& gfx = displayGfx();
  const uint16_t bg = rgb565(8, 10, 18);
  const uint16_t fg = rgb565(255, 220, 80);
  gfx.fillRect(0, SCR_H - 22, SCR_W, 22, bg);
  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(&lgfx::v1::fonts::Font0);
  gfx.setTextColor(fg, bg);
  gfx.setCursor(8, SCR_H - 14);
  gfx.print(msg);
  displayPresent();
}

void displayClearStatus() {
  auto& gfx = displayGfx();
  gfx.fillRect(0, SCR_H - 22, SCR_W, 22, rgb565(8, 10, 18));
  displayPresent();
}

void displaySetFont(lgfx::LGFXBase& gfx, const lgfx::IFont* font) {
  gfx.setFont(font);
  gfx.setTextDatum(textdatum_t::baseline_left);
}

int displayTextWidth(lgfx::LGFXBase& gfx, const char* text) {
  return gfx.textWidth(text);
}

int displayFontAscent(lgfx::LGFXBase& gfx) {
  lgfx::FontMetrics m{};
  gfx.getFont()->getDefaultMetric(&m);
  // baseline ≈ pixels above the baseline for U8g2-backed fonts
  return m.baseline > 0 ? m.baseline : gfx.fontHeight();
}

int displayFontDescent(lgfx::LGFXBase& gfx) {
  lgfx::FontMetrics m{};
  gfx.getFont()->getDefaultMetric(&m);
  // u8g2-style negative descent below baseline
  const int below = m.height - m.baseline;
  return below > 0 ? -below : 0;
}

void displayDrawText(lgfx::LGFXBase& gfx, const char* text, int x, int yBaseline,
                     uint16_t fg, uint16_t bg, const lgfx::IFont* font) {
  displaySetFont(gfx, font);
  gfx.setTextColor(fg, bg);
  gfx.drawString(text, x, yBaseline);
}

void displayDrawUtf8Centered(const char* text, int yBaseline, uint16_t fg,
                             const lgfx::IFont* font) {
  auto& gfx = displayGfx();
  displaySetFont(gfx, font);
  int w = displayTextWidth(gfx, text);
  int x = (SCR_W - w) / 2;
  if (x < 0) x = 0;
  gfx.setTextColor(fg);
  gfx.drawString(text, x, yBaseline);
}
