#include "sdk/flint_face_sdk.h"
#include "faces/glance.h"
#include "config/debug.h"
#include "display/ui_tokens.h"
#include "util/flint_sys.h"

#include <string.h>
#include <stdio.h>

// Built-in reference face. Draws via Face SDK (display + resources).
class DigitalFace : public Face {
public:
  const FaceInfo& info() const override {
    static const FaceInfo i{"digital", "Digital", "FlinT OS", 3, 3};
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
    if (ctx.dayMode != mode_ || force_ || ctx.hour12 != hour12_
        || ctx.showSeconds != showSeconds_) {
      paintFull(ctx);
      return;
    }

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));

    bool weatherDirty = drawWeatherBadge(ctx, false);
    bool timeDirty = false;
    bool dateDirty = false;
    bool wifiDirty = false;
    bool ampmDirty = false;
    bool colonDirty = false;

    if (strcmp(timeStr, lastTime_) != 0) {
      drawTime(ctx, timeStr, false);
      timeDirty = true;
    } else {
      colonDirty = pulseColons(ctx);
    }
    ampmDirty = drawAmpm(ctx, false);
    if (strcmp(dateStr, lastDate_) != 0) {
      drawDate(ctx, dateStr, false);
      dateDirty = true;
    }
    if (ctx.wifiConnected != lastWifi_) {
      drawWifiDot(ctx);
      wifiDirty = true;
    }
    if (!(weatherDirty || timeDirty || dateDirty || wifiDirty || ampmDirty || colonDirty)) {
      return;
    }

    if (!displayHasCanvas()) {
      return;
    }

    DisplayRect rects[5];
    int n = 0;
    displaySetFont(displayGfx(), timeFont());
    const int asc = displayFontAscent(displayGfx());

    if (timeDirty || ampmDirty || colonDirty) {
      rects[n++] = DisplayRect{0, static_cast<int16_t>(UiTok::kTimeY - asc),
                               static_cast<int16_t>(ctx.screenW),
                               static_cast<int16_t>(timeGlyphH_ + 2)};
    }
    if (weatherDirty) {
      rects[n++] = DisplayRect{0, 0, static_cast<int16_t>(UiTok::kWxW + UiTok::kWxX),
                               static_cast<int16_t>(UiTok::kStatusH)};
    }
    if (dateDirty) {
      rects[n++] = DisplayRect{0, static_cast<int16_t>(UiTok::kDateY - 16),
                               static_cast<int16_t>(ctx.screenW),
                               static_cast<int16_t>(UiTok::kDateBandH)};
    }
    if (wifiDirty) {
      rects[n++] = DisplayRect{static_cast<int16_t>(UiTok::wifiX(ctx.screenW)),
                               static_cast<int16_t>(UiTok::kWifiTop),
                               static_cast<int16_t>(UiTok::kWifiSize),
                               static_cast<int16_t>(UiTok::kWifiSize)};
    }

    if (n == 1) {
      displayPresentRect(rects[0].x, rects[0].y, rects[0].w, rects[0].h);
    } else if (n > 1) {
      displayPresentRects(rects, n);
    }
  }

private:
  UiPalette pal_{};
  DayMode mode_ = DAY_MODE_DAY;
  DayModeProfile profile_{};
  bool force_ = true;
  bool showSeconds_ = true;
  bool hour12_ = false;

  char lastTime_[16] = "";
  char lastDate_[32] = "";
  char lastAmpm_[4] = "";
  char lastTemp_[12] = "";
  char lastModeLabel_[16] = "";
  int lastWeatherCode_ = -999;
  bool lastWifi_ = false;
  bool lastWxOk_ = false;
  bool lastWxLoading_ = false;
  uint8_t lastColonFade_ = 255;

  int digitW_ = 0;
  int colonW_ = 0;
  int timeCellX_[8] = {0};
  int timeEndX_ = 0;
  int timeGlyphH_ = 0;
  bool timeLayoutReady_ = false;
  bool layoutWithSeconds_ = true;
  uint8_t timeLen_ = 8;

  static const lgfx::IFont* timeFont() { return flintFont(FlintFontId::TimeLarge); }
  static const lgfx::IFont* bodyFont() { return flintFont(FlintFontId::Body); }

  void resetState() {
    force_ = true;
    timeLayoutReady_ = false;
    lastColonFade_ = 255;
    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = lastAmpm_[0] = '\0';
    lastWeatherCode_ = -999;
  }

  uint16_t pageBg() const { return pal_.bgTop; }

  static int hourForDisplay(int h24, bool hour12) {
    if (!hour12) {
      return h24;
    }
    int h = h24 % 12;
    return h == 0 ? 12 : h;
  }

  void formatTimeDate(const FaceContext& ctx, char* timeStr, size_t timeN,
                      char* dateStr, size_t dateN) {
    if (ctx.hasTime) {
      const int h = hourForDisplay(ctx.localTime.tm_hour, ctx.hour12);
      if (ctx.showSeconds) {
        snprintf(timeStr, timeN, "%02d:%02d:%02d",
                 h, ctx.localTime.tm_min, ctx.localTime.tm_sec);
      } else {
        snprintf(timeStr, timeN, "%02d:%02d", h, ctx.localTime.tm_min);
      }
      snprintf(dateStr, dateN, "%s  %02d.%02d.%04d",
               flintTrWeekday(ctx.localTime.tm_wday),
               ctx.localTime.tm_mday, ctx.localTime.tm_mon + 1,
               ctx.localTime.tm_year + 1900);
    } else {
      snprintf(timeStr, timeN, ctx.showSeconds ? "--:--:--" : "--:--");
      snprintf(dateStr, dateN, "%s", flintTr(FlintStr::WaitingForTime));
    }
  }

  uint8_t colonFadeNow() const {
    const uint32_t phase = flintMillis() % UiTok::kColonPulsePeriodMs;
    // Triangle 255 → ~140 → 255 over the period.
    const uint32_t half = UiTok::kColonPulsePeriodMs / 2;
    uint8_t t;
    if (phase < half) {
      t = static_cast<uint8_t>((phase * 255UL) / half);
    } else {
      t = static_cast<uint8_t>(((UiTok::kColonPulsePeriodMs - phase) * 255UL) / half);
    }
    // Map to soft pulse depth (~140..255).
    return static_cast<uint8_t>(140 + (t * 115UL) / 255);
  }

  uint16_t colonColor(uint8_t fade) const {
    return colorLerp(pal_.textSecondary, pal_.accent, fade);
  }

  void drawStatusHairline(const FaceContext& ctx) {
    auto& gfx = displayGfx();
    gfx.drawFastHLine(UiTok::kPadX, UiTok::kStatusHairlineY,
                      ctx.screenW - UiTok::kPadX * 2, pal_.divider);
  }

  void drawWifiDot(const FaceContext& ctx) {
    flintGlanceDraw(ctx, GLANCE_WIFI,
                    GlanceRect{static_cast<int16_t>(UiTok::wifiX(ctx.screenW)),
                               static_cast<int16_t>(UiTok::kWifiTop),
                               static_cast<int16_t>(UiTok::kWifiSize),
                               static_cast<int16_t>(UiTok::kWifiSize)});
    lastWifi_ = ctx.wifiConnected;
  }

  bool drawWeatherBadge(const FaceContext& ctx, bool force) {
    if (!profile_.showWeather) {
      if (force) {
        auto& gfx = displayGfx();
        gfx.fillRect(UiTok::kWxX, UiTok::kWxY, UiTok::kWxW, UiTok::kWxH, pageBg());
        gfx.fillRect(UiTok::kRangeX, UiTok::kRangeY, UiTok::kRangeW, UiTok::kRangeH, pageBg());
      }
      return force;
    }

    const WeatherState& w = ctx.weather;
    char temp[12];
    if (w.ok) {
      snprintf(temp, sizeof(temp), "%+.0f", static_cast<double>(w.tempC));
    } else if (w.loading) {
      snprintf(temp, sizeof(temp), "..");
    } else {
      snprintf(temp, sizeof(temp), "--");
    }

    const bool dirty = force || (w.ok != lastWxOk_) || (w.loading != lastWxLoading_)
                       || (w.code != lastWeatherCode_) || (strcmp(temp, lastTemp_) != 0);
    if (!dirty) {
      return false;
    }

    flintGlanceDraw(ctx, GLANCE_WEATHER,
                    GlanceRect{static_cast<int16_t>(UiTok::kWxX),
                               static_cast<int16_t>(UiTok::kWxY),
                               static_cast<int16_t>(UiTok::kWxW),
                               static_cast<int16_t>(UiTok::kWxH)});
    if (w.dailyOk) {
      flintGlanceDraw(ctx, GLANCE_DAILY_RANGE,
                      GlanceRect{static_cast<int16_t>(UiTok::kRangeX),
                                 static_cast<int16_t>(UiTok::kRangeY),
                                 static_cast<int16_t>(UiTok::kRangeW),
                                 static_cast<int16_t>(UiTok::kRangeH)});
    } else if (force) {
      auto& gfx = displayGfx();
      gfx.fillRect(UiTok::kRangeX, UiTok::kRangeY, UiTok::kRangeW, UiTok::kRangeH, pageBg());
    }
    lastWeatherCode_ = w.ok ? w.code : -1;
    lastWxOk_ = w.ok;
    lastWxLoading_ = w.loading;
    strncpy(lastTemp_, temp, sizeof(lastTemp_) - 1);
    lastTemp_[sizeof(lastTemp_) - 1] = '\0';
    return true;
  }

  void drawModeLabel(const FaceContext& ctx, bool force) {
    auto& gfx = displayGfx();
    if (!profile_.showModeLabel) {
      if (lastModeLabel_[0] != '\0') {
        gfx.fillRect(ctx.screenW / 2 - 48, 4, 96, 24, pageBg());
        lastModeLabel_[0] = '\0';
      }
      return;
    }
    const char* label = dayModeName(ctx.dayMode);
    if (!force && strcmp(label, lastModeLabel_) == 0) return;

    displaySetFont(gfx, bodyFont());
    int w = displayTextWidth(gfx, label);
    int x = (ctx.screenW - w) / 2;
    int asc = displayFontAscent(gfx);
    int desc = displayFontDescent(gfx);
    gfx.fillRect(ctx.screenW / 2 - 48, UiTok::kModeLabelY - asc, 96, asc - desc + 2, pageBg());
    gfx.setTextColor(pal_.textSecondary, pageBg());
    gfx.drawString(label, x, UiTok::kModeLabelY);
    strncpy(lastModeLabel_, label, sizeof(lastModeLabel_) - 1);
    lastModeLabel_[sizeof(lastModeLabel_) - 1] = '\0';
  }

  void ensureTimeLayout(const FaceContext& ctx, bool withSeconds) {
    if (timeLayoutReady_ && layoutWithSeconds_ == withSeconds) return;

    auto& gfx = displayGfx();
    displaySetFont(gfx, timeFont());
    digitW_ = 0;
    for (char c = '0'; c <= '9'; c++) {
      char t[2] = {c, 0};
      int w = displayTextWidth(gfx, t);
      if (w > digitW_) digitW_ = w;
    }
    {
      char t[2] = {'-', 0};
      int w = displayTextWidth(gfx, t);
      if (w > digitW_) digitW_ = w;
    }
    colonW_ = displayTextWidth(gfx, ":");
    if (colonW_ < 1) colonW_ = digitW_ / 3;
    timeGlyphH_ = displayFontAscent(gfx) - displayFontDescent(gfx) + 2;
    layoutWithSeconds_ = withSeconds;
    timeLen_ = withSeconds ? 8 : 5;

    const int digits = withSeconds ? 6 : 4;
    const int colons = withSeconds ? 2 : 1;
    const int total = digitW_ * digits + colonW_ * colons;
    const int ampmReserve = hour12_ ? 42 : 0;
    int x = (ctx.screenW - total - ampmReserve) / 2;
    if (x < 0) x = 0;
    for (int i = 0; i < timeLen_; i++) {
      timeCellX_[i] = x;
      const bool colonSlot = withSeconds ? (i == 2 || i == 5) : (i == 2);
      x += colonSlot ? colonW_ : digitW_;
    }
    timeEndX_ = x;
    timeLayoutReady_ = true;
  }

  bool drawAmpm(const FaceContext& ctx, bool force) {
    auto& gfx = displayGfx();
    if (!hour12_ || !ctx.hasTime) {
      if (lastAmpm_[0] != '\0') {
        displaySetFont(gfx, bodyFont());
        const int asc = displayFontAscent(gfx);
        const int desc = displayFontDescent(gfx);
        gfx.fillRect(timeEndX_, UiTok::kTimeY - asc, 48, asc - desc + 2, pageBg());
        lastAmpm_[0] = '\0';
        return true;
      }
      return false;
    }

    const char* label = (ctx.localTime.tm_hour >= 12) ? flintTr(FlintStr::PM) : flintTr(FlintStr::AM);
    if (!force && strcmp(label, lastAmpm_) == 0) {
      return false;
    }

    displaySetFont(gfx, bodyFont());
    const int asc = displayFontAscent(gfx);
    const int desc = displayFontDescent(gfx);
    const int x = timeEndX_ + UiTok::kGapSm;
    gfx.fillRect(x, UiTok::kTimeY - asc, 40, asc - desc + 2, pageBg());
    gfx.setTextColor(pal_.textSecondary, pageBg());
    gfx.drawString(label, x, UiTok::kTimeY - 4);
    strncpy(lastAmpm_, label, sizeof(lastAmpm_) - 1);
    lastAmpm_[sizeof(lastAmpm_) - 1] = '\0';
    return true;
  }

  void drawTimeGlyph(lgfx::LGFXBase& gfx, int index, char ch, uint16_t fg, uint16_t bg) {
    displaySetFont(gfx, timeFont());
    const bool isColon = (ch == ':');
    const int cellW = isColon ? colonW_ : digitW_;
    const int x = timeCellX_[index];
    const int asc = displayFontAscent(gfx);
    gfx.fillRect(x, UiTok::kTimeY - asc, cellW, timeGlyphH_, bg);
    char tmp[2] = {ch, 0};
    int gw = displayTextWidth(gfx, tmp);
    gfx.setTextColor(fg, bg);
    gfx.drawString(tmp, x + (cellW - gw) / 2, UiTok::kTimeY);
  }

  bool pulseColons(const FaceContext& ctx) {
    if (!ctx.hasTime || lastTime_[0] == '\0') {
      return false;
    }
    const uint8_t fade = colonFadeNow();
    // Quantize to avoid SPI spam.
    if (static_cast<int>(fade) - static_cast<int>(lastColonFade_) < 12
        && static_cast<int>(lastColonFade_) - static_cast<int>(fade) < 12) {
      return false;
    }
    lastColonFade_ = fade;
    auto& gfx = displayGfx();
    const uint16_t fg = colonColor(fade);
    for (int i = 0; lastTime_[i] != '\0'; i++) {
      if (lastTime_[i] == ':') {
        drawTimeGlyph(gfx, i, ':', fg, pageBg());
      }
    }
    return true;
  }

  void drawTime(const FaceContext& ctx, const char* timeStr, bool force) {
    if (!force && strcmp(timeStr, lastTime_) == 0) return;
    auto& gfx = displayGfx();
    ensureTimeLayout(ctx, showSeconds_);
    if (force || (lastTime_[0] != '\0' && strlen(lastTime_) != strlen(timeStr))) {
      displaySetFont(gfx, timeFont());
      int asc = displayFontAscent(gfx);
      gfx.fillRect(0, UiTok::kTimeY - asc, ctx.screenW, timeGlyphH_ + 2, pageBg());
      lastTime_[0] = '\0';
    }
    lastColonFade_ = colonFadeNow();
    const uint16_t cFg = colonColor(lastColonFade_);
    for (int i = 0; timeStr[i] != '\0'; i++) {
      if (force || lastTime_[i] == '\0' || timeStr[i] != lastTime_[i]) {
        const uint16_t fg = (timeStr[i] == ':') ? cFg : pal_.accent;
        drawTimeGlyph(gfx, i, timeStr[i], fg, pageBg());
        lastTime_[i] = timeStr[i];
      }
    }
    lastTime_[strlen(timeStr)] = '\0';
  }

  void drawDate(const FaceContext& ctx, const char* dateStr, bool force) {
    if (!force && strcmp(dateStr, lastDate_) == 0) return;
    (void)dateStr;
    flintGlanceDraw(ctx, GLANCE_DATE,
                    GlanceRect{0, static_cast<int16_t>(UiTok::kDateY - 16),
                               static_cast<int16_t>(ctx.screenW),
                               static_cast<int16_t>(UiTok::kDateBandH)});
    char buf[32];
    if (ctx.hasTime) {
      snprintf(buf, sizeof(buf), "%s  %02d.%02d.%04d",
               flintTrWeekday(ctx.localTime.tm_wday), ctx.localTime.tm_mday,
               ctx.localTime.tm_mon + 1, ctx.localTime.tm_year + 1900);
    } else {
      snprintf(buf, sizeof(buf), "%s", flintTr(FlintStr::WaitingForTime));
    }
    strcpy(lastDate_, buf);
  }

  void paintFull(const FaceContext& ctx) {
    mode_ = ctx.dayMode;
    profile_ = ctx.dayProfile;
    pal_ = ctx.palette;
    showSeconds_ = ctx.showSeconds;
    hour12_ = ctx.hour12;
    timeLayoutReady_ = false;

    LOGD("face", "digital full mode=%s canvas=%d", dayModeName(ctx.dayMode),
         (int)displayHasCanvas());

    auto& gfx = displayGfx();
    gfx.fillScreen(pageBg());
    const int wash = profile_.washHeight;
    const int y0 = ctx.screenH - wash;
    for (int y = y0; y < ctx.screenH; y++) {
      uint8_t t = wash > 1 ? (uint8_t)(((y - y0) * 100) / wash) : 0;
      gfx.drawFastHLine(0, y, ctx.screenW, colorLerp(pageBg(), pal_.bgBottom, t));
    }

    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = lastAmpm_[0] = '\0';
    lastWeatherCode_ = -999;
    lastWxOk_ = !ctx.weather.ok;
    lastWxLoading_ = !ctx.weather.loading;
    lastColonFade_ = 255;

    drawWifiDot(ctx);
    drawModeLabel(ctx, true);
    drawWeatherBadge(ctx, true);
    drawStatusHairline(ctx);

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));
    drawTime(ctx, timeStr, true);
    drawAmpm(ctx, true);
    drawDate(ctx, dateStr, true);

    displayPresent();
    force_ = false;
  }
};

Face* createDigitalFace() {
  return new DigitalFace();
}
