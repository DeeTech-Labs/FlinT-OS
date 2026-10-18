#include "sdk/flint_face_sdk.h"
#include "sdk/companion.h"
#include <cstring>
#include <cstdio>
namespace {
const FaceInfo metadata{"companion","PC Companion","DeeTech Labs",1,1};
class CompanionFace : public Face {
public:
  const FaceInfo& info() const override { return metadata; }
  void onForceRedraw(const FaceContext& ctx) override { paint(ctx); }
  void onTick(const FaceContext& ctx) override { if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION)) return; paint(ctx); }
private:
  void paint(const FaceContext& ctx) {
    auto& gfx = displayGfx(); const auto& palette = ctx.palette; const auto& state = companionState();
    gfx.fillScreen(palette.bgTop);
    const char* title = state.mode == CompanionMode::Focus ? "Focus" : state.mode == CompanionMode::Computer ? "PC Companion" : "Quick note";
    displayDrawUtf8Centered(title,38,palette.textSecondary,flintFontBody());
    char text[97]{};
    if (state.mode == CompanionMode::Focus) {
      const auto seconds = companionFocusRemaining();
      snprintf(text,sizeof(text),"%02lu:%02lu",static_cast<unsigned long>(seconds/60),static_cast<unsigned long>(seconds%60));
      displayDrawUtf8Centered(text,130,palette.accent,flintFontTimeLarge());
      if (!seconds) displayDrawUtf8Centered("Complete",180,palette.textSecondary,flintFontBody());
    } else if (state.mode == CompanionMode::Computer) {
      snprintf(text,sizeof(text),"CPU: %d%%",state.cpuPercent); displayDrawUtf8Centered(state.cpuPercent < 0 ? "CPU: --" : text,100,palette.accent,flintFontBody());
      snprintf(text,sizeof(text),"RAM: %d%%",state.memoryPercent); displayDrawUtf8Centered(state.memoryPercent < 0 ? "RAM: --" : text,145,palette.accent,flintFontBody());
    } else {
      displaySetFont(gfx,flintFontBody());
      const char* source = state.note[0] ? state.note : "Send a note from Desktop";
      char line[97]{}; size_t count = 0; int y = 80;
      for (size_t index = 0; source[index] && y <= ctx.screenH-20;) {
        const unsigned char first = source[index];
        size_t bytes = first < 0x80 ? 1 : (first & 0xe0) == 0xc0 ? 2 : (first & 0xf0) == 0xe0 ? 3 : 4;
        if (index + bytes > strlen(source)) break;
        if (count + bytes >= sizeof(line)) break;
        memcpy(line+count,source+index,bytes); line[count+bytes] = 0;
        if (displayTextWidth(gfx,line) > ctx.screenW-32 && count) {
          line[count] = 0; displayDrawText(gfx,line,16,y,palette.textPrimary,palette.bgTop,flintFontBody()); y += 28; count = 0;
        } else { count += bytes; index += bytes; }
      }
      if (count && y <= ctx.screenH-20) displayDrawText(gfx,line,16,y,palette.textPrimary,palette.bgTop,flintFontBody());
    }
    displayPresent();
  }
};
}
Face* createCompanionFace() { return new CompanionFace; }
const FaceInfo& companionFaceInfo() { return metadata; }
