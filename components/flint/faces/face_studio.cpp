#include "sdk/flint_face_sdk.h"
#include "sdk/studio.h"
#include <cstdio>
namespace {
const FaceInfo metadata{"studio","Face Studio","DeeTech Labs",1,1};
uint16_t color(uint32_t rgb) { return rgb565(rgb >> 16, rgb >> 8, rgb); }
class StudioFace : public Face {
public:
  const FaceInfo& info() const override { return metadata; }
  void onForceRedraw(const FaceContext& ctx) override { paint(ctx); }
  void onTick(const FaceContext& ctx) override {
    if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION)) return;
    paint(ctx);
  }
private:
  void paint(const FaceContext& ctx) {
    const auto& definition = studioDefinition();
    const auto background = color(definition.background), foreground = color(definition.foreground);
    auto& gfx = displayGfx(); gfx.fillScreen(background);
    for (int bit = 0; bit < 256; ++bit)
      if (definition.pixels[bit / 8] & (0x80 >> (bit % 8)))
        gfx.fillRect((ctx.screenW-48)/2 + (bit % 16)*3,16 + (bit/16)*3,3,3,foreground);
    char time[24] = "--:--";
    if (ctx.hasTime) strftime(time,sizeof(time),ctx.showSeconds ? (ctx.hour12 ? "%I:%M:%S" : "%H:%M:%S") : (ctx.hour12 ? "%I:%M" : "%H:%M"),&ctx.localTime);
    displayDrawUtf8Centered(time,definition.timeY,foreground,ctx.showSeconds ? flintFontTimeCompact() : flintFontTimeLarge());
    if (ctx.hour12 && ctx.hasTime) displayDrawUtf8Centered(ctx.localTime.tm_hour < 12 ? "AM" : "PM",definition.timeY+24,foreground,flintFontBody());
    if (definition.showDate) {
      char date[96]; flintFormatDateLine(date,sizeof(date),ctx.hasTime,ctx.localTime,ctx.dateFormat,ctx.dateSep);
      displayDrawUtf8Centered(date,definition.dateY,foreground,flintFontBody());
    }
    displayPresent();
  }
};
}
Face* createStudioFace() { return new StudioFace; }
const FaceInfo& studioFaceInfo() { return metadata; }
