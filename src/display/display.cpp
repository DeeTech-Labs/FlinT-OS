#include "display.h"
#include "src/config/config.h"
#include "src/config/debug.h"

#include <SPI.h>

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g;

int SCR_W = 320;
int SCR_H = 240;

void displayBegin() {
  SPI.begin(TFT_SCLK, /*MISO*/ -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(TFT_ROTATION);
  SCR_W = tft.width();
  SCR_H = tft.height();
  tft.setTextWrap(false);
  u8g.begin(tft);
  LOGI("disp", "ST7789 ready %dx%d", SCR_W, SCR_H);
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
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
  for (int y = 0; y < SCR_H; y++) {
    uint8_t t = (uint8_t)((y * 255) / (SCR_H - 1));
    tft.drawFastHLine(0, y, SCR_W, colorLerp(top, bottom, t));
  }
}

void displayShowStatus(const char* msg) {
  tft.fillRect(0, SCR_H - 22, SCR_W, 22, rgb565(8, 10, 18));
  tft.setTextColor(rgb565(255, 220, 80), rgb565(8, 10, 18));
  tft.setTextSize(1);
  tft.setCursor(8, SCR_H - 14);
  tft.print(msg);
}

void displayClearStatus() {
  // status bar is part of UI redraw; keep a dark strip for boot messages
  tft.fillRect(0, SCR_H - 22, SCR_W, 22, rgb565(8, 10, 18));
}

void displayDrawUtf8(const char* text, int x, int y, uint16_t fg, const uint8_t* font) {
  u8g.setFontMode(1);
  u8g.setFont(font);
  u8g.setForegroundColor(fg);
  u8g.setCursor(x, y);
  u8g.print(text);
}

int displayUtf8Width(const char* text, const uint8_t* font) {
  u8g.setFont(font);
  return u8g.getUTF8Width(text);
}

void displayDrawUtf8Centered(const char* text, int y, uint16_t fg, const uint8_t* font) {
  int w = displayUtf8Width(text, font);
  int x = (SCR_W - w) / 2;
  if (x < 0) x = 0;
  displayDrawUtf8(text, x, y, fg, font);
}
