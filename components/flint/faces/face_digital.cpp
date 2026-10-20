#include "render/builders.h"
#include "render/watch_display.h"
#include "sdk/flint_face_sdk.h"
#include "settings/settings.h"
#include <cstdio>
namespace {
const FaceInfo metadata{"digital", "Digital", "DeeTech Labs", 2, 1};
class DigitalFace : public Face {
public:
  const FaceInfo &info() const override { return metadata; }
  void onForceRedraw(const FaceContext &ctx) override { paint(ctx); }
  void onTick(const FaceContext &ctx) override {
    if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION))
      return;
    paint(ctx);
  }

private:
  void paint(const FaceContext &ctx) {
    using namespace FlintRender;
    auto s = begin("FlinT Spark");
    char main[16] = "--:--", seconds[8] = "--", date[96];
    if (ctx.hasTime) {
      strftime(main, sizeof(main), ctx.hour12 ? "%I:%M" : "%H:%M",
               &ctx.localTime);
      strftime(seconds, sizeof(seconds), "%S", &ctx.localTime);
    }
    const int mainWidth = textWidth(
        TimeLarge, reinterpret_cast<const uint8_t *>(main), strlen(main));
    const int secondsWidth =
        ctx.showSeconds
            ? textWidth(Body, reinterpret_cast<const uint8_t *>(seconds),
                        strlen(seconds)) +
                  10
            : 0;
    const int x = (280 - mainWidth - secondsWidth) / 2;
    text(s, x, 137, main, White, TimeLarge);
    if (ctx.showSeconds)
      text(s, x + mainWidth + 10, 137, seconds, Accent);
    if (ctx.hour12)
      text(s, 240, 76, ctx.hasTime && ctx.localTime.tm_hour >= 12 ? "PM" : "AM",
           Muted);
    flintFormatDateLine(date, sizeof(date), ctx.hasTime, ctx.localTime,
                        ctx.dateFormat, ctx.dateSep);
    char fitted[96];
    fit(fitted, sizeof(fitted), date, 248);
    text(s, 140, 184, fitted, Muted, Body, true);
    rect(s, 115, 207, 50, 3, Accent, 1);
    watchPresentScene(s);
  }
};
} // namespace
Face *createDigitalFace() { return new DigitalFace; }
const FaceInfo &digitalFaceInfo() { return metadata; }
