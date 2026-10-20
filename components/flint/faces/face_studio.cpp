#include "render/builders.h"
#include "render/watch_display.h"
#include "sdk/flint_face_sdk.h"
#include "sdk/studio.h"
#include <cstdio>
namespace {
const FaceInfo metadata{"studio", "Face Studio", "DeeTech Labs", 1, 1};
class StudioFace : public Face {
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
    const auto &definition = studioDefinition();
    using namespace FlintRender;
    char time[24] = "--:--", date[96] = "";
    if (ctx.hasTime)
      strftime(time, sizeof(time),
               ctx.showSeconds ? (ctx.hour12 ? "%I:%M:%S" : "%H:%M:%S")
                               : (ctx.hour12 ? "%I:%M" : "%H:%M"),
               &ctx.localTime);
    if (definition.showDate)
      flintFormatDateLine(date, sizeof(date), ctx.hasTime, ctx.localTime,
                          ctx.dateFormat, ctx.dateSep);
    auto scene = studio(
        definition.background, definition.foreground,
        definition.animateIcon && studioAnimationFrame()
            ? definition.alternatePixels
            : definition.pixels,
        definition.iconX, definition.iconY, definition.iconScale,
        definition.showIcon, definition.layerOrder, time,
        ctx.showSeconds ? TimeCompact : TimeLarge, definition.timeY, date,
        definition.dateY,
        ctx.hour12 && ctx.hasTime ? (ctx.localTime.tm_hour < 12 ? "AM" : "PM")
                                  : "");
    watchPresentScene(scene);
  }
};
} // namespace
Face *createStudioFace() { return new StudioFace; }
const FaceInfo &studioFaceInfo() { return metadata; }
