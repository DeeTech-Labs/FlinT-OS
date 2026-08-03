#pragma once

#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <stdint.h>

extern Adafruit_ST7789 tft;
extern U8G2_FOR_ADAFRUIT_GFX u8g;
extern int SCR_W;
extern int SCR_H;

void displayBegin();
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t colorLerp(uint16_t a, uint16_t b, uint8_t t /*0..255*/);

void displayFillGradientV(uint16_t top, uint16_t bottom);
void displayShowStatus(const char* msg);
void displayClearStatus();

void displayDrawUtf8(const char* text, int x, int y, uint16_t fg, const uint8_t* font);
int displayUtf8Width(const char* text, const uint8_t* font);
void displayDrawUtf8Centered(const char* text, int y, uint16_t fg, const uint8_t* font);
