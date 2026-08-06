#include "sdk/flint_face_sdk.h"
#include "faces/glance.h"
#include "config/debug.h"
#include "display/ui_tokens.h"
#include "time/date_fmt.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <string.h>
#include <stdio.h>

// Built-in reference face. Draws via Face SDK (display + resources).
static const FaceInfo kDigitalFaceInfo{"digital", "Digital", "DeeTech Labs",
                                       FLINT_FACE_DIGITAL_VERSION,
                                       FLINT_FACE_DIGITAL_MIN_API};

class DigitalFace : public Face {
public:
  const FaceInfo& info() const override {
    return kDigitalFaceInfo;
  }

  void onActivate(const FaceContext& ctx) override {
    (void)ctx;
    resetState();
  }

  void onForceRedraw(const FaceContext& ctx) override {
    paintFull(ctx);
  }

  void onTick(const FaceContext& ctx) override {
    // Motion-only: pulse colons without reformatting time/weather/palette work.
    if (ctx.dirty != 0 && (ctx.dirty & ~UI_DIRTY_MOTION) == 0) {
      if (mode_ != ctx.dayMode || force_ || !timeLayoutReady_) {
        paintFull(ctx);
        return;
      }
      if (!pulseColons(ctx)) {
        return;
      }
      if (!displayHasCanvas()) {
        return;
      }
      DisplayRect rects[2];
      const int n = appendColonRects(ctx, timeAsc_, rects, 2);
      if (n == 1) {
        displayPresentRect(rects[0].x, rects[0].y, rects[0].w, rects[0].h);
      } else if (n > 1) {
        displayPresentRects(rects, n);
      }
      return;
    }

    if (ctx.dayMode != mode_ || force_ || ctx.hour12 != hour12_
        || ctx.showSeconds != showSeconds_
        || ctx.dateFormat != dateFormat_ || ctx.dateSep != dateSep_) {
      paintFull(ctx);
      return;
    }

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));
    ensureTimeLayout(ctx, showSeconds_);

    displaySetFont(displayGfx(), timeFont());
    const int asc = timeAsc_;
    const DisplayRect timeBand = tightTimeBand(ctx, asc);
    const DisplayRect ampmBand = tightAmpmBand(ctx, asc);
    const DisplayRect weatherBand = tightWeatherBand(ctx);
    const DisplayRect wifiBand{static_cast<int16_t>(UiTok::wifiX(ctx.screenW)),
                               static_cast<int16_t>(UiTok::wifiTop(ctx.screenH)),
                               static_cast<int16_t>(UiTok::kWifiSize),
                               static_cast<int16_t>(UiTok::kWifiSize)};

    displaySetClipRect(weatherBand.x, weatherBand.y, weatherBand.w, weatherBand.h);
    bool weatherDirty = drawWeatherBadge(ctx, false);
    bool timeDirty = false;
    bool dateDirty = false;
    bool wifiDirty = false;
    bool ampmDirty = false;
    bool colonDirty = false;

    DisplayRect dateBand{};
    displaySetClipRect(timeBand.x, timeBand.y,
                       static_cast<int16_t>(timeBand.w + (hour12_ ? ampmBand.w + UiTok::kGapSm : 0)),
                       timeBand.h);
    if (strcmp(timeStr, lastTime_) != 0) {
      drawTime(ctx, timeStr, false);
      timeDirty = true;
    } else {
      colonDirty = pulseColons(ctx);
    }
    ampmDirty = drawAmpm(ctx, false);

    if (strcmp(dateStr, lastDate_) != 0) {
      dateBand = tightDateBand(ctx, dateStr);
      displaySetClipRect(dateBand.x, dateBand.y, dateBand.w, dateBand.h);
      flintGlanceDraw(ctx, GLANCE_DATE,
                      GlanceRect{dateBand.x, dateBand.y, dateBand.w, dateBand.h}, dateStr);
      strncpy(lastDate_, dateStr, sizeof(lastDate_) - 1);
      lastDate_[sizeof(lastDate_) - 1] = '\0';
      dateDirty = true;
    }
    if (ctx.wifiBars != lastWifiBars_) {
      displaySetClipRect(wifiBand.x, wifiBand.y, wifiBand.w, wifiBand.h);
      drawWifiDot(ctx);
      wifiDirty = true;
    }
    displayClearClipRect();

    if (!(weatherDirty || timeDirty || dateDirty || wifiDirty || ampmDirty || colonDirty)) {
      return;
    }

    if (!displayHasCanvas()) {
      return;
    }

    DisplayRect rects[6];
    int n = 0;
    if (colonDirty && !timeDirty && !ampmDirty) {
      n = appendColonRects(ctx, asc, rects, 6);
    } else if (timeDirty) {
      DisplayRect band = timeBand;
      if (hour12_ || ampmDirty) {
        const int right = ampmBand.x + ampmBand.w;
        band.w = static_cast<int16_t>(right - band.x);
      }
      rects[n++] = band;
    } else if (ampmDirty) {
      rects[n++] = ampmBand;
    }
    if (weatherDirty) {
      rects[n++] = weatherBand;
    }
    if (dateDirty) {
      rects[n++] = dateBand;
    }
    if (wifiDirty) {
      rects[n++] = wifiBand;
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
  uint8_t dateFormat_ = SETTINGS_DATE_DMY4;
  uint8_t dateSep_ = SETTINGS_DATE_SEP_DOT;

  char lastTime_[16] = "";
  char lastDate_[32] = "";
  char lastAmpm_[4] = "";
  char lastTemp_[12] = "";
  char lastModeLabel_[16] = "";
  int lastWeatherCode_ = -999;
  uint8_t lastWifiBars_ = 0xFF;
  bool lastWxOk_ = false;
  bool lastWxLoading_ = false;
  uint8_t lastColonFade_ = 255;
  int lastDatePresentW_ = 0;

  int digitW_ = 0;
  int colonW_ = 0;
  int glyphWDigit_[10] = {0};
  int glyphWDash_ = 0;
  int timeCellX_[8] = {0};
  int timeEndX_ = 0;
  int timeGlyphH_ = 0;
  int timeAsc_ = 0;
  bool timeLayoutReady_ = false;
  bool layoutWithSeconds_ = true;
  uint8_t timeLen_ = 8;

  static const lgfx::IFont* timeFont() { return flintFont(FlintFontId::TimeLarge); }
  static const lgfx::IFont* bodyFont() { return flintFont(FlintFontId::Body); }

  void resetState() {
    force_ = true;
    timeLayoutReady_ = false;
    lastColonFade_ = 255;
    lastDatePresentW_ = 0;
    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = lastAmpm_[0] = '\0';
    lastWeatherCode_ = -999;
  }

  DisplayRect tightTimeBand(const FaceContext& ctx, int asc) const {
    const int x = timeLayoutReady_ ? timeCellX_[0] : UiTok::contentLeft();
    const int w = timeLayoutReady_ ? (timeEndX_ - x) : UiTok::contentWidth(ctx.screenW);
    return DisplayRect{static_cast<int16_t>(x),
                       static_cast<int16_t>(UiTok::timeY(ctx.screenH) - asc),
                       static_cast<int16_t>(w),
                       static_cast<int16_t>(timeGlyphH_ > 0 ? timeGlyphH_ + 2 : 40)};
  }

  DisplayRect tightAmpmBand(const FaceContext& ctx, int asc) const {
    const int x = timeEndX_ + UiTok::kGapSm;
    return DisplayRect{static_cast<int16_t>(x),
                       static_cast<int16_t>(UiTok::timeY(ctx.screenH) - asc), 48,
                       static_cast<int16_t>(timeGlyphH_ > 0 ? timeGlyphH_ + 2 : 24)};
  }

  DisplayRect tightWeatherBand(const FaceContext& ctx) const {
    const int x = UiTok::wxX(ctx.screenW);
    const int y = UiTok::wxY(ctx.screenH);
    const int w = UiTok::wxW(ctx.screenW) > UiTok::rangeW(ctx.screenW)
                      ? UiTok::wxW(ctx.screenW)
                      : UiTok::rangeW(ctx.screenW);
    const int h = (UiTok::rangeY(ctx.screenH) + UiTok::rangeH(ctx.screenH)) - y;
    return DisplayRect{static_cast<int16_t>(x), static_cast<int16_t>(y),
                       static_cast<int16_t>(w), static_cast<int16_t>(h > 0 ? h : UiTok::statusH(ctx.screenH))};
  }

  DisplayRect tightDateBand(const FaceContext& ctx, const char* dateStr) {
    auto& gfx = displayGfx();
    displaySetFont(gfx, bodyFont());
    const int tw = displayTextWidth(gfx, dateStr);
    const int bandH = UiTok::dateBandH(ctx.screenH);
    int presentW = tw + 16;
    if (presentW < lastDatePresentW_) {
      presentW = lastDatePresentW_;
    }
    if (presentW > ctx.screenW) {
      presentW = ctx.screenW;
    }
    int x = (ctx.screenW - presentW) / 2;
    if (x < 0) {
      x = 0;
    }
    lastDatePresentW_ = tw + 16;
    return DisplayRect{static_cast<int16_t>(x),
                       static_cast<int16_t>(UiTok::dateY(ctx.screenH) - 16),
                       static_cast<int16_t>(presentW), static_cast<int16_t>(bandH)};
  }

  int appendColonRects(const FaceContext& ctx, int asc, DisplayRect* out, int cap) const {
    int n = 0;
    const int ty = UiTok::timeY(ctx.screenH) - asc;
    const int h = timeGlyphH_ > 0 ? timeGlyphH_ + 2 : 40;
    for (int i = 0; lastTime_[i] != '\0' && n < cap; i++) {
      if (lastTime_[i] != ':') {
        continue;
      }
      out[n++] = DisplayRect{static_cast<int16_t>(timeCellX_[i]), static_cast<int16_t>(ty),
                             static_cast<int16_t>(colonW_ > 0 ? colonW_ : digitW_),
                             static_cast<int16_t>(h)};
    }
    return n;
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
    } else {
      snprintf(timeStr, timeN, ctx.showSeconds ? "--:--:--" : "--:--");
    }
    flintFormatDateLine(dateStr, dateN, ctx.hasTime, ctx.localTime, ctx.dateFormat,
                        ctx.dateSep);
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
    gfx.drawFastHLine(UiTok::contentLeft(), UiTok::statusHairlineY(ctx.screenH),
                      UiTok::contentWidth(ctx.screenW), pal_.divider);
  }

  void drawWifiDot(const FaceContext& ctx) {
    flintGlanceDraw(ctx, GLANCE_WIFI,
                    GlanceRect{static_cast<int16_t>(UiTok::wifiX(ctx.screenW)),
                               static_cast<int16_t>(UiTok::wifiTop(ctx.screenH)),
                               static_cast<int16_t>(UiTok::kWifiSize),
                               static_cast<int16_t>(UiTok::kWifiSize)});
    lastWifiBars_ = ctx.wifiBars;
  }

  bool drawWeatherBadge(const FaceContext& ctx, bool force) {
    const int wx = UiTok::wxX(ctx.screenW);
    const int wy = UiTok::wxY(ctx.screenH);
    const int ww = UiTok::wxW(ctx.screenW);
    const int wh = UiTok::wxH(ctx.screenH);
    const int rx = UiTok::rangeX(ctx.screenW);
    const int ry = UiTok::rangeY(ctx.screenH);
    const int rw = UiTok::rangeW(ctx.screenW);
    const int rh = UiTok::rangeH(ctx.screenH);

    if (!profile_.showWeather) {
      if (force) {
        auto& gfx = displayGfx();
        gfx.fillRect(wx, wy, ww, wh, pageBg());
        gfx.fillRect(rx, ry, rw, rh, pageBg());
      }
      return force;
    }

    if (!ctx.weather) {
      return force;
    }
    const WeatherState& w = *ctx.weather;
    char temp[12];
    weatherFormatTempC(temp, sizeof(temp), w);

    const bool dirty = force || (w.ok != lastWxOk_) || (w.loading != lastWxLoading_)
                       || (w.code != lastWeatherCode_) || (strcmp(temp, lastTemp_) != 0);
    if (!dirty) {
      return false;
    }

    flintGlanceDraw(ctx, GLANCE_WEATHER,
                    GlanceRect{static_cast<int16_t>(wx), static_cast<int16_t>(wy),
                               static_cast<int16_t>(ww), static_cast<int16_t>(wh)},
                    temp);
    if (w.dailyOk) {
      flintGlanceDraw(ctx, GLANCE_DAILY_RANGE,
                      GlanceRect{static_cast<int16_t>(rx), static_cast<int16_t>(ry),
                                 static_cast<int16_t>(rw), static_cast<int16_t>(rh)});
    } else if (force) {
      auto& gfx = displayGfx();
      gfx.fillRect(rx, ry, rw, rh, pageBg());
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
    const int maxW = UiTok::modeLabelMaxW(ctx.screenW);
    const int clearX = (ctx.screenW - maxW) / 2;
    const int labelY = UiTok::modeLabelY(ctx.screenH);
    if (!profile_.showModeLabel) {
      if (lastModeLabel_[0] != '\0') {
        gfx.fillRect(clearX, 4, maxW, 24, pageBg());
        lastModeLabel_[0] = '\0';
      }
      return;
    }
    const char* label = dayModeName(ctx.dayMode);
    if (!force && strcmp(label, lastModeLabel_) == 0) return;

    displaySetFont(gfx, bodyFont());
    int w = displayTextWidth(gfx, label);
    if (w > maxW) w = maxW;
    int x = (ctx.screenW - w) / 2;
    int asc = displayFontAscent(gfx);
    int desc = displayFontDescent(gfx);
    gfx.fillRect(clearX, labelY - asc, maxW, asc - desc + 2, pageBg());
    gfx.setTextColor(pal_.textSecondary, pageBg());
    gfx.drawString(label, x, labelY);
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
      const int w = displayTextWidth(gfx, t);
      glyphWDigit_[c - '0'] = w;
      if (w > digitW_) digitW_ = w;
    }
    {
      char t[2] = {'-', 0};
      glyphWDash_ = displayTextWidth(gfx, t);
      if (glyphWDash_ > digitW_) digitW_ = glyphWDash_;
    }
    colonW_ = displayTextWidth(gfx, ":");
    if (colonW_ < 1) colonW_ = digitW_ / 3;
    timeAsc_ = displayFontAscent(gfx);
    timeGlyphH_ = timeAsc_ - displayFontDescent(gfx) + 2;
    layoutWithSeconds_ = withSeconds;
    timeLen_ = withSeconds ? 8 : 5;

    const int digits = withSeconds ? 6 : 4;
    const int colons = withSeconds ? 2 : 1;
    const int ampmReserve = hour12_ ? 42 : 0;
    const int avail = UiTok::contentWidth(ctx.screenW) - ampmReserve;
    int total = digitW_ * digits + colonW_ * colons;
    // Squeeze cell widths if the hero clock would overflow the safe area.
    if (total > avail && digits > 0) {
      const int over = total - avail;
      const int shrink = (over + digits - 1) / digits;
      digitW_ -= shrink;
      if (digitW_ < 8) digitW_ = 8;
      total = digitW_ * digits + colonW_ * colons;
    }
    int x = (ctx.screenW - total - ampmReserve) / 2;
    if (x < UiTok::contentLeft()) x = UiTok::contentLeft();
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
    const int ty = UiTok::timeY(ctx.screenH);
    if (!hour12_ || !ctx.hasTime) {
      if (lastAmpm_[0] != '\0') {
        displaySetFont(gfx, bodyFont());
        const int asc = displayFontAscent(gfx);
        const int desc = displayFontDescent(gfx);
        gfx.fillRect(timeEndX_, ty - asc, 48, asc - desc + 2, pageBg());
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
    gfx.fillRect(x, ty - asc, 40, asc - desc + 2, pageBg());
    gfx.setTextColor(pal_.textSecondary, pageBg());
    gfx.drawString(label, x, ty - 4);
    strncpy(lastAmpm_, label, sizeof(lastAmpm_) - 1);
    lastAmpm_[sizeof(lastAmpm_) - 1] = '\0';
    return true;
  }

  void drawTimeGlyph(lgfx::LGFXBase& gfx, int index, char ch, uint16_t fg, uint16_t bg,
                     int screenH) {
    displaySetFont(gfx, timeFont());
    const bool isColon = (ch == ':');
    const int cellW = isColon ? colonW_ : digitW_;
    const int x = timeCellX_[index];
    const int ty = UiTok::timeY(screenH);
    gfx.fillRect(x, ty - timeAsc_, cellW, timeGlyphH_, bg);
    char tmp[2] = {ch, 0};
    int gw = colonW_;
    if (!isColon) {
      if (ch >= '0' && ch <= '9') {
        gw = glyphWDigit_[ch - '0'];
      } else if (ch == '-') {
        gw = glyphWDash_;
      } else {
        gw = digitW_;
      }
    }
    gfx.setTextColor(fg, bg);
    gfx.drawString(tmp, x + (cellW - gw) / 2, ty);
  }

  bool pulseColons(const FaceContext& ctx) {
    // Motion gated: no colon pulse when seconds are off (shell skips MOTION too).
    if (!ctx.showSeconds || !ctx.hasTime || lastTime_[0] == '\0') {
      return false;
    }
    const uint8_t fade = colonFadeNow();
    // Quantize to avoid SPI spam (~11 Hz ticks × coarse steps).
    if (static_cast<int>(fade) - static_cast<int>(lastColonFade_) < 16
        && static_cast<int>(lastColonFade_) - static_cast<int>(fade) < 16) {
      return false;
    }
    lastColonFade_ = fade;
    auto& gfx = displayGfx();
    const uint16_t fg = colonColor(fade);
    for (int i = 0; lastTime_[i] != '\0'; i++) {
      if (lastTime_[i] == ':') {
        drawTimeGlyph(gfx, i, ':', fg, pageBg(), ctx.screenH);
      }
    }
    return true;
  }

  void drawTime(const FaceContext& ctx, const char* timeStr, bool force) {
    if (!force && strcmp(timeStr, lastTime_) == 0) return;
    auto& gfx = displayGfx();
    ensureTimeLayout(ctx, showSeconds_);
    const int ty = UiTok::timeY(ctx.screenH);
    if (force || (lastTime_[0] != '\0' && strlen(lastTime_) != strlen(timeStr))) {
      displaySetFont(gfx, timeFont());
      gfx.fillRect(0, ty - timeAsc_, ctx.screenW, timeGlyphH_ + 2, pageBg());
      lastTime_[0] = '\0';
    }
    lastColonFade_ = colonFadeNow();
    const uint16_t cFg = colonColor(lastColonFade_);
    for (int i = 0; timeStr[i] != '\0'; i++) {
      if (force || lastTime_[i] == '\0' || timeStr[i] != lastTime_[i]) {
        const uint16_t fg = (timeStr[i] == ':') ? cFg : pal_.accent;
        drawTimeGlyph(gfx, i, timeStr[i], fg, pageBg(), ctx.screenH);
        lastTime_[i] = timeStr[i];
      }
    }
    lastTime_[strlen(timeStr)] = '\0';
  }

  void drawDate(const FaceContext& ctx, const char* dateStr, bool force) {
    if (!force && strcmp(dateStr, lastDate_) == 0) {
      return;
    }
    DisplayRect band;
    if (force) {
      lastDatePresentW_ = 0;
      (void)tightDateBand(ctx, dateStr);
      band = DisplayRect{0, static_cast<int16_t>(UiTok::dateY(ctx.screenH) - 16),
                         static_cast<int16_t>(ctx.screenW),
                         static_cast<int16_t>(UiTok::dateBandH(ctx.screenH))};
    } else {
      band = tightDateBand(ctx, dateStr);
    }
    flintGlanceDraw(ctx, GLANCE_DATE, GlanceRect{band.x, band.y, band.w, band.h}, dateStr);
    strncpy(lastDate_, dateStr, sizeof(lastDate_) - 1);
    lastDate_[sizeof(lastDate_) - 1] = '\0';
  }

  void paintFull(const FaceContext& ctx) {
    mode_ = ctx.dayMode;
    profile_ = ctx.dayProfile;
    pal_ = ctx.palette;
    showSeconds_ = ctx.showSeconds;
    hour12_ = ctx.hour12;
    dateFormat_ = ctx.dateFormat;
    dateSep_ = ctx.dateSep;
    timeLayoutReady_ = false;

    LOGD("face", "digital full mode=%s canvas=%d", dayModeName(ctx.dayMode),
         (int)displayHasCanvas());

    displayClearClipRect();
    auto& gfx = displayGfx();
    gfx.fillScreen(pageBg());
    dayModeDrawWash(ctx.screenW, ctx.screenH, pageBg(), pal_.bgBottom, profile_.washHeight);

    lastTime_[0] = lastDate_[0] = lastTemp_[0] = lastModeLabel_[0] = lastAmpm_[0] = '\0';
    lastWeatherCode_ = -999;
    lastWxOk_ = !(ctx.weather && ctx.weather->ok);
    lastWxLoading_ = !(ctx.weather && ctx.weather->loading);
    lastColonFade_ = 255;
    lastDatePresentW_ = 0;

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

const FaceInfo& digitalFaceInfo() {
  return kDigitalFaceInfo;
}
