#include "render/watch_faces.h"
#include "render/watch_display.h"
#include "sdk/flint_face_sdk.h"
#include <cstdio>
namespace {
const FaceInfo digital{"digital", "Spark Base", "DeeTech Labs", 3, 1};
class ClockFace : public Face {
 public:
  ClockFace(const FaceInfo &metadata, FlintRender::ClockStyle style)
      : metadata_(metadata), style_(style) {}
  const FaceInfo &info() const override { return metadata_; }
  void onForceRedraw(const FaceContext &ctx) override { paint(ctx); }
  void onTick(const FaceContext &ctx) override {
    if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION)) return;
    // Formatting the date only on minute/settings changes avoids repeated
    // localization and width fitting for a second tick.
    paint(ctx);
  }
 private:
  const FaceInfo &metadata_;
  FlintRender::ClockStyle style_;
  char time_[16] = "--:--", hours_[4] = "--", minutes_[4] = "--", date_[96]{};
  bool haveText_ = false;
  void paint(const FaceContext &ctx) {
    if (!haveText_ || ctx.dirty != UI_DIRTY_SECOND) {
      strcpy(time_, "--:--"); strcpy(hours_, "--"); strcpy(minutes_, "--");
      if (ctx.hasTime) {
        strftime(time_, sizeof(time_), ctx.hour12 ? "%I:%M" : "%H:%M", &ctx.localTime);
        strftime(hours_, sizeof(hours_), ctx.hour12 ? "%I" : "%H", &ctx.localTime);
        strftime(minutes_, sizeof(minutes_), "%M", &ctx.localTime);
      }
      char date[96];
      flintFormatDateLine(date, sizeof(date), ctx.hasTime, ctx.localTime,
                          ctx.dateFormat, ctx.dateSep);
      FlintRender::fit(date_, sizeof(date_), date, 236);
      haveText_ = true;
    }
    char seconds[4] = "--";
    if (ctx.hasTime) snprintf(seconds, sizeof(seconds), "%02d", ctx.localTime.tm_sec);
    const char *meridiem = ctx.hasTime && ctx.hour12 ?
        (ctx.localTime.tm_hour >= 12 ? "PM" : "AM") : "";
    watchPresentScene(FlintRender::clockScene(style_, {time_, hours_, minutes_,
        seconds, date_, ctx.usbConnected ? "USB" : "", meridiem, ctx.showSeconds}));
  }
};
} // namespace
Face *createDigitalFace() { return new ClockFace(digital, FlintRender::ClockStyle::Digital); }
const FaceInfo &digitalFaceInfo() { return digital; }
