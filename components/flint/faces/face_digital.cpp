#include "sdk/flint_face_sdk.h"
#include "faces/face_digital_layout.h"
#include "faces/glance.h"
#include "config/debug.h"
#include "display/ui_tokens.h"
#include "time/date_fmt.h"
#include "ui/ui_dirty.h"

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
    if (ctx.dirty != 0 && (ctx.dirty & ~UI_DIRTY_MOTION) == 0) return;

    // SETTINGS is soft (no FORCE): rebuild when prefs/i18n may have changed.
    if ((ctx.dirty & UI_DIRTY_SETTINGS) != 0 || ctx.dayMode != mode_ || force_
        || ctx.hour12 != hour12_ || ctx.showSeconds != showSeconds_
        || ctx.dateFormat != dateFormat_ || ctx.dateSep != dateSep_) {
      paintFull(ctx);
      return;
    }

    char timeStr[16];
    char dateStr[32];
    formatTimeDate(ctx, timeStr, sizeof(timeStr), dateStr, sizeof(dateStr));
    timeLayout_.ensure(ctx, showSeconds_, hour12_, timeFont());

    const DisplayRect timeBand = timeLayout_.timeBand(ctx.screenH);
    const DisplayRect ampmBand = timeLayout_.ampmBand(ctx.screenH);

    bool timeDirty = false;
    bool dateDirty = false;
    bool ampmDirty = false;
    char prevTime[16];
    memcpy(prevTime, lastTime_, sizeof(prevTime));

    DisplayRect dateBand{};
    displaySetClipRect(timeBand.x, timeBand.y,
                       static_cast<int16_t>(timeBand.w + (hour12_ ? ampmBand.w + UiTok::kGapSm : 0)),
                       timeBand.h);
    if (strcmp(timeStr, lastTime_) != 0) {
      drawTime(ctx, timeStr, false);
      timeDirty = true;
    }
    ampmDirty = drawAmpm(ctx, false);

    if (strcmp(dateStr, lastDate_) != 0) {
      dateBand = digitalDateBand(ctx, dateStr, &lastDatePresentW_, bodyFont());
      displaySetClipRect(dateBand.x, dateBand.y, dateBand.w, dateBand.h);
      flintGlanceDraw(ctx, GLANCE_DATE,
                      GlanceRect{dateBand.x, dateBand.y, dateBand.w, dateBand.h}, dateStr);
      strncpy(lastDate_, dateStr, sizeof(lastDate_) - 1);
      lastDate_[sizeof(lastDate_) - 1] = '\0';
      dateDirty = true;
    }
    displayClearClipRect();

    if (!(timeDirty || dateDirty || ampmDirty)) {
      return;
    }

    if (!displayHasCanvas()) {
      return;
    }

    // Up to 8 time glyphs + AM/PM + date.
    DisplayRect rects[11];
    int n = 0;
    if (timeDirty) {
      n = timeLayout_.appendChangedGlyphRects(ctx.screenH, prevTime, lastTime_, rects, 8);
    }
    if (ampmDirty && n < 11) {
      rects[n++] = ampmBand;
    }
    if (dateDirty && n < 11) {
      rects[n++] = dateBand;
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
  bool force_ = true;
  bool showSeconds_ = true;
  bool hour12_ = false;
  uint8_t dateFormat_ = SETTINGS_DATE_DMY4;
  uint8_t dateSep_ = SETTINGS_DATE_SEP_DOT;

  char lastTime_[16] = "";
  char lastDate_[32] = "";
  char lastAmpm_[4] = "";
  int lastDatePresentW_ = 0;

  DigitalTimeLayout timeLayout_{};

  const lgfx::IFont* timeFont() const {
    return flintFont(hour12_ ? FlintFontId::TimeCompact : FlintFontId::TimeLarge);
  }
  static const lgfx::IFont* bodyFont() { return flintFont(FlintFontId::Body); }

  void resetState() {
    force_ = true;
    timeLayout_.reset();
    lastDatePresentW_ = 0;
    lastTime_[0] = lastDate_[0] = lastAmpm_[0] = '\0';
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

  bool drawAmpm(const FaceContext& ctx, bool force) {
    auto& gfx = displayGfx();
    const int ty = UiTok::timeY(ctx.screenH);
    if (!hour12_ || !ctx.hasTime) {
      if (lastAmpm_[0] != '\0') {
        displaySetFont(gfx, bodyFont());
        const int asc = displayFontAscent(gfx);
        const int desc = displayFontDescent(gfx);
        gfx.fillRect(timeLayout_.endX, ty - asc, UiTok::kAmpmSlotW,
                     asc - desc + UiTok::kTimeGlyphPadY, pageBg());
        lastAmpm_[0] = '\0';
        return true;
      }
      return false;
    }

    const char* label =
        (ctx.localTime.tm_hour >= 12) ? flintTr(FlintStr::PM) : flintTr(FlintStr::AM);
    if (!force && strcmp(label, lastAmpm_) == 0) {
      return false;
    }

    displaySetFont(gfx, bodyFont());
    const int asc = displayFontAscent(gfx);
    const int desc = displayFontDescent(gfx);
    const int x = timeLayout_.endX + UiTok::kGapSm;
    gfx.fillRect(x, ty - asc, UiTok::kAmpmSlotW, asc - desc + UiTok::kTimeGlyphPadY, pageBg());
    gfx.setTextColor(pal_.textSecondary, pageBg());
    gfx.drawString(label, x, ty - UiTok::kAmpmBaselineLift);
    strncpy(lastAmpm_, label, sizeof(lastAmpm_) - 1);
    lastAmpm_[sizeof(lastAmpm_) - 1] = '\0';
    return true;
  }

  void drawTime(const FaceContext& ctx, const char* timeStr, bool force) {
    if (!force && strcmp(timeStr, lastTime_) == 0) {
      return;
    }
    auto& gfx = displayGfx();
    timeLayout_.ensure(ctx, showSeconds_, hour12_, timeFont());
    displaySetFont(gfx, timeFont());
    const int ty = UiTok::timeY(ctx.screenH);
    if (force || (lastTime_[0] != '\0' && strlen(lastTime_) != strlen(timeStr))) {
      gfx.fillRect(0, ty - timeLayout_.ascent, ctx.screenW,
                   timeLayout_.glyphH + UiTok::kTimeGlyphPadY, pageBg());
      lastTime_[0] = '\0';
    }
    const uint16_t cFg = colorLerp(pageBg(), pal_.accent, 150);
    for (int i = 0; timeStr[i] != '\0'; i++) {
      if (force || lastTime_[i] == '\0' || timeStr[i] != lastTime_[i]) {
        const uint16_t fg = (i >= 5) ? colorLerp(pageBg(), pal_.textSecondary, 170)
                            : (timeStr[i] == ':' ? cFg : pal_.textPrimary);
        timeLayout_.drawGlyph(gfx, i, timeStr[i], fg, pageBg(), ctx.screenH);
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
      (void)digitalDateBand(ctx, dateStr, &lastDatePresentW_, bodyFont());
      band = DisplayRect{0,
                         static_cast<int16_t>(UiTok::dateY(ctx.screenH) - UiTok::kDateBandAscentPad),
                         static_cast<int16_t>(ctx.screenW),
                         static_cast<int16_t>(UiTok::dateBandH(ctx.screenH))};
    } else {
      band = digitalDateBand(ctx, dateStr, &lastDatePresentW_, bodyFont());
    }
    flintGlanceDraw(ctx, GLANCE_DATE, GlanceRect{band.x, band.y, band.w, band.h}, dateStr);
    strncpy(lastDate_, dateStr, sizeof(lastDate_) - 1);
    lastDate_[sizeof(lastDate_) - 1] = '\0';
  }

  void paintFull(const FaceContext& ctx) {
    mode_ = ctx.dayMode;
    pal_ = ctx.palette;
    showSeconds_ = ctx.showSeconds;
    hour12_ = ctx.hour12;
    dateFormat_ = ctx.dateFormat;
    dateSep_ = ctx.dateSep;
    timeLayout_.reset();

    LOGD("face", "digital full mode=%s canvas=%d", dayModeName(ctx.dayMode),
         (int)displayHasCanvas());

    displayClearClipRect();
    auto& gfx = displayGfx();
    gfx.fillScreen(pageBg());


    lastTime_[0] = lastDate_[0] = lastAmpm_[0] = '\0';
    lastDatePresentW_ = 0;


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
