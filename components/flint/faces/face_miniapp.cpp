#include "companion/miniapps.h"
#include "render/watch_faces.h"
#include "render/watch_display.h"
#include "sdk/flint_face_sdk.h"
namespace {
class MiniappFace : public Face {
  MiniappDefinition definition_;
  FaceInfo metadata_;
 public:
  explicit MiniappFace(const MiniappDefinition& p) : definition_(p),
      metadata_{definition_.id,definition_.name,"Miniapp",p.version,1} {}
  const FaceInfo& info() const override { return metadata_; }
  void onForceRedraw(const FaceContext& ctx) override { paint(ctx); }
  void onTick(const FaceContext& ctx) override {
    if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION)) return;
    paint(ctx);
  }
 private:
  void paint(const FaceContext& ctx) {
    char time[16]="--:--", hours[4]="--", minutes[4]="--", seconds[4]="--", date[96];
    if(ctx.hasTime) {
      strftime(time,sizeof(time),ctx.hour12?"%I:%M":"%H:%M",&ctx.localTime);
      strftime(hours,sizeof(hours),ctx.hour12?"%I":"%H",&ctx.localTime);
      strftime(minutes,sizeof(minutes),"%M",&ctx.localTime);
      strftime(seconds,sizeof(seconds),"%S",&ctx.localTime);
    }
    flintFormatDateLine(date,sizeof(date),ctx.hasTime,ctx.localTime,ctx.dateFormat,ctx.dateSep);
    const auto* c=definition_.colors;
    FlintRender::ThemeColors theme{FlintRender::rgb24(c[0]),FlintRender::rgb24(c[1]),
        FlintRender::rgb24(c[2]),FlintRender::rgb24(c[3]),FlintRender::rgb24(c[4]),FlintRender::rgb24(c[5])};
    watchPresentScene(FlintRender::clockScene(static_cast<FlintRender::ClockStyle>(definition_.layout),
        {time,hours,minutes,seconds,date,ctx.usbConnected?"USB":"",
         ctx.hasTime&&ctx.hour12?(ctx.localTime.tm_hour>=12?"PM":"AM"):"",ctx.showSeconds},theme));
  }
};
}
Face* createMiniappFace(const MiniappDefinition& definition) { return new MiniappFace(definition); }
