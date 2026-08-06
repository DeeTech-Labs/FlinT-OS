#include "display.h"
#include "config/config.h"
#include "config/debug.h"
#include "util/flint_sys.h"

LGFX tft;
static LGFX_Sprite gCanvas(&tft);
static bool gHasCanvas = false;

// Full RGB565 backbuffer in BSS — heap on ESP32-C3 often has no contiguous
// large block after Wi-Fi/IDF init. Partitions.csv does not affect RAM.
// Landscape max for 240×280 panel: 280×240.
static constexpr int kFbMaxW = 280;
static constexpr int kFbMaxH = 240;
alignas(16) static uint16_t s_framebuf[kFbMaxW * kFbMaxH];

int SCR_W = 280;
int SCR_H = 240;

static bool clampRect(int& x, int& y, int& w, int& h) {
  if (w <= 0 || h <= 0) {
    return false;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > SCR_W) {
    w = SCR_W - x;
  }
  if (y + h > SCR_H) {
    h = SCR_H - y;
  }
  return w > 0 && h > 0;
}

static void pushRectRaw(const uint16_t* buf, int x, int y, int w, int h) {
  tft.startWrite();
  for (int row = 0; row < h; row++) {
    const uint16_t* src = buf + (size_t)(y + row) * (size_t)SCR_W + x;
    tft.setAddrWindow(x, y + row, w, 1);
    tft.writePixels(src, w);
  }
  tft.endWrite();
}

void displayBegin() {
  // Panel init keeps DISPON off (see Panel_ST7789_NoDispOn) so empty GRAM
  // never flashes white under invert.
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.setColorDepth(16);
  SCR_W = tft.width();
  SCR_H = tft.height();
  tft.setTextWrap(false);

  const uint16_t bootBg = rgb565(8, 10, 18);
  tft.fillScreen(bootBg);
  tft.startWrite();
  tft.writeCommand(0x29);  // DISPON — first visible frame is already brand bg
  tft.endWrite();

  if (SCR_W > kFbMaxW || SCR_H > kFbMaxH) {
    gHasCanvas = false;
    LOGE("disp", "panel %dx%d exceeds static FB %dx%d", SCR_W, SCR_H, kFbMaxW, kFbMaxH);
  } else {
    gCanvas.setColorDepth(16);
    gCanvas.setBuffer(s_framebuf, SCR_W, SCR_H, 16);
    gCanvas.setTextWrap(false);
    gHasCanvas = true;
    LOGI("disp", "ST7789 %dx%d canvas BSS=%u heap=%u", SCR_W, SCR_H,
         (unsigned)(sizeof(s_framebuf)), (unsigned)flintFreeHeap());
  }

  if (!gHasCanvas) {
    tft.fillScreen(rgb565(8, 10, 18));
    tft.setTextDatum(textdatum_t::middle_center);
    tft.setFont(&lgfx::v1::fonts::Font2);
    tft.setTextColor(rgb565(255, 120, 100));
    tft.drawString("Low RAM", SCR_W / 2, SCR_H / 2 - 10);
    tft.setTextColor(rgb565(160, 170, 180));
    tft.drawString("canvas OOM", SCR_W / 2, SCR_H / 2 + 14);
    LOGE("disp", "ST7789 canvas OOM heap=%u — UI halted", (unsigned)flintFreeHeap());
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
  gCanvas.pushSprite(&tft, 0, 0);
}

void displayPresentRect(int x, int y, int w, int h) {
  if (!gHasCanvas) {
    return;
  }
  if (!clampRect(x, y, w, h)) {
    return;
  }
  const uint16_t* buf = static_cast<const uint16_t*>(gCanvas.getBuffer());
  if (!buf) {
    displayPresent();
    return;
  }
  pushRectRaw(buf, x, y, w, h);
}

void displayPresentRects(const DisplayRect* rects, int count) {
  if (!gHasCanvas || !rects || count <= 0) {
    return;
  }
  const uint16_t* buf = static_cast<const uint16_t*>(gCanvas.getBuffer());
  if (!buf) {
    displayPresent();
    return;
  }

  // Single transaction for multiple dirty bands.
  tft.startWrite();
  for (int i = 0; i < count; i++) {
    int x = rects[i].x;
    int y = rects[i].y;
    int w = rects[i].w;
    int h = rects[i].h;
    if (!clampRect(x, y, w, h)) {
      continue;
    }
    for (int row = 0; row < h; row++) {
      const uint16_t* src = buf + (size_t)(y + row) * (size_t)SCR_W + x;
      tft.setAddrWindow(x, y + row, w, 1);
      tft.writePixels(src, w);
    }
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
  return m.baseline > 0 ? m.baseline : gfx.fontHeight();
}

int displayFontDescent(lgfx::LGFXBase& gfx) {
  lgfx::FontMetrics m{};
  gfx.getFont()->getDefaultMetric(&m);
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
