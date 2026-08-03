#include "src/faces/Face.h"
#include "src/faces/FaceRegistry.h"
#include "src/display/display.h"
#include "src/config/debug.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

// Built-in reference face. Community faces copy this pattern.
class DigitalFace : public Face {
public:
  const FaceInfo& info() const override {
    static const FaceInfo i{"digital", "Digital", "FlinT OS", 1, 1};
    return i;
  }

  void onActivate(const FaceContext& ctx) override {
    (void)ctx;
    resetState();
  }

  void onForceRedraw(const FaceContext& ctx) override {
    paintFull(ctx);
  }

  void onTick(const FaceContext& ctx) override {
    if (ctx.dayMode != mode_ || force_) {
      paintFull(ctx);
      return;
    }

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));

    drawWeatherBadge(ctx, false);
    if (strcmp(timeStr, lastTime_) != 0) {
      drawTime(ctx, timeStr, false);
    }
    if (strcmp(dateStr, lastDate_) != 0) {
      drawDate(ctx, dateStr, false);
    }
    if (ctx.wifiConnected != lastWifi_) {
      drawWifiDot(ctx);
    }
  }

private:
  static const int WX_ICON_CX = 18;
  static const int WX_ICON_CY = 16;
  static const int WX_TEMP_X = 34;
  static const int WX_TEMP_Y = 20;
  static const int TIME_Y = 125;
  static const int DATE_Y = 168;
  static const int MODE_LABEL_Y = 20;

  UiPalette pal_{};
  DayMode mode_ = DAY_MODE_DAY;
  DayModeProfile profile_{};
  bool force_ = true;

  char lastTime_[16] = "";
  char lastDate_[32] = "";
  char lastTemp_[12] = "";
  char lastModeLabel_[16] = "";
  int lastWeatherCode_ = -999;
  bool lastWifi_ = false;
  bool lastWxOk_ = false;
  bool lastWxLoading_ = false;

  int digitW_ = 0;
  int colonW_ = 0;
  int timeCellX_[8] = {0};
  int timeGlyphH_ = 0;
  bool timeLayoutReady_ = false;
  bool layoutWithSeconds_ = true;
  uint8_t timeLen_ = 8;

  static const uint8_t* timeFont() { return u8g2_font_logisoso46_tn; }

  static const char* WEEKDAYS_RU[];

  void resetState() {
    force_ = true;
    timeLayoutReady_ = false;
    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = '\0';
    lastWeatherCode_ = -999;
  }

  uint16_t pageBg() const { return pal_.bgTop; }

  void formatTimeDate(const FaceContext& ctx, char* timeStr, size_t timeN,
                      char* dateStr, size_t dateN) {
    if (ctx.hasTime) {
      if (profile_.showSeconds) {
        snprintf(timeStr, timeN, "%02d:%02d:%02d",
                 ctx.localTime.tm_hour, ctx.localTime.tm_min, ctx.localTime.tm_sec);
      } else {
        snprintf(timeStr, timeN, "%02d:%02d",
                 ctx.localTime.tm_hour, ctx.localTime.tm_min);
      }
      snprintf(dateStr, dateN, "%s  %02d.%02d.%04d",
               WEEKDAYS_RU[ctx.localTime.tm_wday],
               ctx.localTime.tm_mday, ctx.localTime.tm_mon + 1,
               ctx.localTime.tm_year + 1900);
    } else {
      snprintf(timeStr, timeN, profile_.showSeconds ? "--:--:--" : "--:--");
      snprintf(dateStr, dateN, "ожидание времени");
    }
  }

  void drawOpaqueDiff(const char* now, char* prev, int x, int y,
                      const uint8_t* font, uint16_t fg, uint16_t bg) {
    u8g.setFontMode(0);
    u8g.setFont(font);
    u8g.setForegroundColor(fg);
    u8g.setBackgroundColor(bg);

    const bool force = (prev[0] == '\0');
    int cx = x;
    for (int i = 0; now[i] != '\0'; i++) {
      char tmp[2] = {now[i], 0};
      int cw = u8g.getUTF8Width(tmp);
      if (force || now[i] != prev[i]) {
        u8g.setCursor(cx, y);
        u8g.print(now[i]);
        prev[i] = now[i];
      }
      cx += cw;
    }
    if (strlen(now) < strlen(prev)) {
      int clearX = cx;
      for (int i = (int)strlen(now); prev[i] != '\0'; i++) {
        char tmp[2] = {prev[i], 0};
        clearX += u8g.getUTF8Width(tmp);
      }
      int leftover = clearX - cx;
      if (leftover > 0) {
        int asc = u8g.getFontAscent();
        int desc = u8g.getFontDescent();
        tft.fillRect(cx, y - asc, leftover, asc - desc + 2, bg);
      }
    }
    prev[strlen(now)] = '\0';
  }

  void drawWeatherIconSmall(int cx, int cy, int code, uint16_t color, uint16_t bg) {
    tft.fillRect(cx - 10, cy - 10, 20, 20, bg);
    if (code < 0) {
      tft.drawCircle(cx, cy, 6, color);
      return;
    }
    if (code == 0) {
      tft.fillCircle(cx, cy, 4, color);
      for (int a = 0; a < 8; a++) {
        float ang = a * 0.785398f;
        tft.drawPixel(cx + (int)(8 * cosf(ang)), cy + (int)(8 * sinf(ang)), color);
      }
      return;
    }
    if (code <= 3) {
      tft.fillCircle(cx - 2, cy + 1, 4, color);
      tft.fillCircle(cx + 3, cy + 1, 5, color);
      tft.fillCircle(cx, cy - 2, 3, color);
      return;
    }
    if (code == 45 || code == 48) {
      tft.drawFastHLine(cx - 7, cy - 2, 14, color);
      tft.drawFastHLine(cx - 5, cy + 2, 10, color);
      return;
    }
    if (code >= 71 && code <= 86) {
      tft.drawLine(cx, cy - 5, cx, cy + 5, color);
      tft.drawLine(cx - 4, cy - 2, cx + 4, cy + 2, color);
      tft.drawLine(cx - 4, cy + 2, cx + 4, cy - 2, color);
      return;
    }
    if (code >= 95) {
      tft.fillTriangle(cx - 3, cy - 5, cx + 4, cy - 5, cx, cy + 1, color);
      tft.fillTriangle(cx, cy - 1, cx + 3, cy - 1, cx - 3, cy + 6, color);
      return;
    }
    tft.fillCircle(cx - 2, cy - 2, 4, color);
    tft.fillCircle(cx + 3, cy - 1, 4, color);
    tft.drawLine(cx - 3, cy + 4, cx - 4, cy + 7, color);
    tft.drawLine(cx + 2, cy + 4, cx + 1, cy + 7, color);
  }

  void drawWifiDot(const FaceContext& ctx) {
    tft.fillCircle(ctx.screenW - 14, 14, 4,
                   ctx.wifiConnected ? pal_.wifiOn : pal_.wifiOff);
    lastWifi_ = ctx.wifiConnected;
  }

  void drawModeLabel(const FaceContext& ctx, bool force) {
    if (!profile_.showModeLabel) {
      if (lastModeLabel_[0] != '\0') {
        tft.fillRect(ctx.screenW / 2 - 40, 4, 80, 22, pageBg());
        lastModeLabel_[0] = '\0';
      }
      return;
    }
    const char* label = profile_.nameRu;
    if (!force && strcmp(label, lastModeLabel_) == 0) return;

    u8g.setFont(u8g2_font_10x20_t_cyrillic);
    int w = u8g.getUTF8Width(label);
    int x = (ctx.screenW - w) / 2;
    int asc = u8g.getFontAscent();
    int desc = u8g.getFontDescent();
    tft.fillRect(ctx.screenW / 2 - 48, MODE_LABEL_Y - asc, 96, asc - desc + 2, pageBg());
    u8g.setFontMode(0);
    u8g.setForegroundColor(pal_.textSecondary);
    u8g.setBackgroundColor(pageBg());
    u8g.setCursor(x, MODE_LABEL_Y);
    u8g.print(label);
    strncpy(lastModeLabel_, label, sizeof(lastModeLabel_) - 1);
    lastModeLabel_[sizeof(lastModeLabel_) - 1] = '\0';
  }

  void drawWeatherBadge(const FaceContext& ctx, bool force) {
    if (!profile_.showWeather) {
      if (force) tft.fillRect(0, 0, 100, 28, pageBg());
      return;
    }

    char temp[12];
    int iconCode = -1;
    const WeatherState& w = ctx.weather;
    if (w.ok) {
      snprintf(temp, sizeof(temp), "%+.0f", w.tempC);
      iconCode = w.code;
    } else if (w.loading) {
      snprintf(temp, sizeof(temp), "..");
    } else {
      snprintf(temp, sizeof(temp), "--");
    }

    bool iconDirty = force || (w.ok != lastWxOk_) || (w.loading != lastWxLoading_)
                     || (iconCode != lastWeatherCode_);
    if (iconDirty) {
      tft.fillRect(0, 0, 100, 28, pageBg());
      drawWeatherIconSmall(WX_ICON_CX, WX_ICON_CY, iconCode, pal_.accent, pageBg());
      lastWeatherCode_ = iconCode;
      lastWxOk_ = w.ok;
      lastWxLoading_ = w.loading;
      lastTemp_[0] = '\0';
    }

    if (lastTemp_[0] == '\0' || strcmp(temp, lastTemp_) != 0) {
      if (lastTemp_[0] != '\0' && strlen(temp) < strlen(lastTemp_)) {
        u8g.setFont(u8g2_font_9x15_tn);
        int asc = u8g.getFontAscent();
        int desc = u8g.getFontDescent();
        tft.fillRect(WX_TEMP_X, WX_TEMP_Y - asc, 60, asc - desc + 2, pageBg());
        lastTemp_[0] = '\0';
      }
      if (lastTemp_[0] == '\0') {
        char blank[12] = "";
        drawOpaqueDiff(temp, blank, WX_TEMP_X, WX_TEMP_Y,
                       u8g2_font_9x15_tn, pal_.textSecondary, pageBg());
        strcpy(lastTemp_, temp);
      } else {
        drawOpaqueDiff(temp, lastTemp_, WX_TEMP_X, WX_TEMP_Y,
                       u8g2_font_9x15_tn, pal_.textSecondary, pageBg());
      }
    }
  }

  void ensureTimeLayout(const FaceContext& ctx, bool withSeconds) {
    if (timeLayoutReady_ && layoutWithSeconds_ == withSeconds) return;

    u8g.setFont(timeFont());
    digitW_ = 0;
    for (char c = '0'; c <= '9'; c++) {
      char t[2] = {c, 0};
      int w = u8g.getUTF8Width(t);
      if (w > digitW_) digitW_ = w;
    }
    {
      char t[2] = {'-', 0};
      int w = u8g.getUTF8Width(t);
      if (w > digitW_) digitW_ = w;
    }
    colonW_ = u8g.getUTF8Width(":");
    if (colonW_ < 1) colonW_ = digitW_ / 3;
    timeGlyphH_ = u8g.getFontAscent() - u8g.getFontDescent() + 2;
    layoutWithSeconds_ = withSeconds;
    timeLen_ = withSeconds ? 8 : 5;

    const int digits = withSeconds ? 6 : 4;
    const int colons = withSeconds ? 2 : 1;
    const int total = digitW_ * digits + colonW_ * colons;
    int x = (ctx.screenW - total) / 2;
    if (x < 0) x = 0;
    for (int i = 0; i < timeLen_; i++) {
      timeCellX_[i] = x;
      const bool colonSlot = withSeconds ? (i == 2 || i == 5) : (i == 2);
      x += colonSlot ? colonW_ : digitW_;
    }
    timeLayoutReady_ = true;
  }

  void drawTimeGlyph(int index, char ch, uint16_t fg, uint16_t bg) {
    u8g.setFont(timeFont());
    const bool isColon = (ch == ':');
    const int cellW = isColon ? colonW_ : digitW_;
    const int x = timeCellX_[index];
    const int asc = u8g.getFontAscent();
    tft.fillRect(x, TIME_Y - asc, cellW, timeGlyphH_, bg);
    char tmp[2] = {ch, 0};
    int gw = u8g.getUTF8Width(tmp);
    u8g.setFontMode(0);
    u8g.setForegroundColor(fg);
    u8g.setBackgroundColor(bg);
    u8g.setCursor(x + (cellW - gw) / 2, TIME_Y);
    u8g.print(ch);
  }

  void drawTime(const FaceContext& ctx, const char* timeStr, bool force) {
    if (!force && strcmp(timeStr, lastTime_) == 0) return;
    ensureTimeLayout(ctx, profile_.showSeconds);
    if (force || (lastTime_[0] != '\0' && strlen(lastTime_) != strlen(timeStr))) {
      u8g.setFont(timeFont());
      int asc = u8g.getFontAscent();
      tft.fillRect(0, TIME_Y - asc, ctx.screenW, timeGlyphH_ + 2, pageBg());
      lastTime_[0] = '\0';
    }
    for (int i = 0; timeStr[i] != '\0'; i++) {
      if (force || lastTime_[i] == '\0' || timeStr[i] != lastTime_[i]) {
        drawTimeGlyph(i, timeStr[i], pal_.accent, pageBg());
        lastTime_[i] = timeStr[i];
      }
    }
    lastTime_[strlen(timeStr)] = '\0';
  }

  void drawDate(const FaceContext& ctx, const char* dateStr, bool force) {
    if (!force && strcmp(dateStr, lastDate_) == 0) return;
    u8g.setFont(u8g2_font_10x20_t_cyrillic);
    int w = u8g.getUTF8Width(dateStr);
    int x = (ctx.screenW - w) / 2;
    int asc = u8g.getFontAscent();
    int desc = u8g.getFontDescent();
    tft.fillRect(0, DATE_Y - asc, ctx.screenW, asc - desc + 2, pageBg());
    u8g.setFontMode(0);
    u8g.setForegroundColor(pal_.textSecondary);
    u8g.setBackgroundColor(pageBg());
    u8g.setCursor(x, DATE_Y);
    u8g.print(dateStr);
    strcpy(lastDate_, dateStr);
  }

  void paintFull(const FaceContext& ctx) {
    mode_ = ctx.dayMode;
    profile_ = ctx.dayProfile;
    pal_ = ctx.palette;
    timeLayoutReady_ = false;

    LOGD("face", "digital full mode=%s", profile_.nameRu);

    tft.fillScreen(pageBg());
    const int wash = profile_.washHeight;
    const int y0 = ctx.screenH - wash;
    for (int y = y0; y < ctx.screenH; y++) {
      uint8_t t = wash > 1 ? (uint8_t)(((y - y0) * 90) / wash) : 0;
      tft.drawFastHLine(0, y, ctx.screenW, colorLerp(pageBg(), pal_.bgBottom, t));
    }

    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = '\0';
    lastWeatherCode_ = -999;
    lastWxOk_ = !ctx.weather.ok;
    lastWxLoading_ = !ctx.weather.loading;

    drawWifiDot(ctx);
    drawModeLabel(ctx, true);
    drawWeatherBadge(ctx, true);

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));
    drawTime(ctx, timeStr, true);
    drawDate(ctx, dateStr, true);

    force_ = false;
  }
};

const char* DigitalFace::WEEKDAYS_RU[] = {
  "Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"
};

static Face* createDigitalFace() {
  return new DigitalFace();
}

FLINT_REGISTER_FACE(createDigitalFace);
