#include "render/builders.h"
#include "render/watch_display.h"
#include "sdk/companion.h"
#include "sdk/flint_face_sdk.h"
#include "settings/settings.h"
#include <cstdio>
namespace {
const FaceInfo metadata{"companion", "PC Companion", "DeeTech Labs", 2, 1};
class CompanionFace : public Face {
public:
  const FaceInfo &info() const override { return metadata; }
  void onForceRedraw(const FaceContext &ctx) override { paint(ctx); }
  void onTick(const FaceContext &ctx) override {
    if (ctx.dirty && !(ctx.dirty & ~UI_DIRTY_MOTION))
      return;
    paint(ctx);
  }

private:
  void paint(const FaceContext &) {
    using namespace FlintRender;
    const auto &state = companionState();
    const bool ru = settingsGet().lang == 0;
    auto s = begin(state.mode == CompanionMode::Focus ? (ru ? "Фокус" : "Focus")
                   : state.mode == CompanionMode::Computer
                       ? "PC Companion"
                       : (ru ? "Заметка" : "Quick note"));
    char textValue[97];
    if (state.mode == CompanionMode::Focus) {
      const auto remaining = companionFocusRemaining();
      snprintf(textValue, sizeof(textValue), "%02lu:%02lu",
               static_cast<unsigned long>(remaining / 60),
               static_cast<unsigned long>(remaining % 60));
      const auto font = strlen(textValue) > 5 ? TimeCompact : TimeLarge;
      text(s, 140, 139, textValue, White, font, true);
      text(s, 140, 192,
           remaining ? (ru ? "Время для важного" : "Time to focus")
                     : (ru ? "Готово" : "Complete"),
           Muted, Body, true);
    } else if (state.mode == CompanionMode::Computer) {
      snprintf(textValue, sizeof(textValue),
               state.cpuPercent < 0 ? "CPU   —" : "CPU   %d%%",
               state.cpuPercent);
      text(s, 20, 90, textValue, White);
      rect(s, 20, 105, 240, 7, Surface, 3);
      if (state.cpuPercent > 0)
        rect(s, 20, 105, 240 * state.cpuPercent / 100, 7, Accent, 3);
      snprintf(textValue, sizeof(textValue),
               state.memoryPercent < 0 ? "RAM   —" : "RAM   %d%%",
               state.memoryPercent);
      text(s, 20, 159, textValue, White);
      rect(s, 20, 174, 240, 7, Surface, 3);
      if (state.memoryPercent > 0)
        rect(s, 20, 174, 240 * state.memoryPercent / 100, 7, Accent, 3);
    } else {
      const char *note = state.note[0] ? state.note
                                       : (ru ? "Отправь заметку из Desktop"
                                             : "Send a note from Desktop");
      size_t cursor = 0, start = 0;
      int y = 79;
      while (note[cursor] && y <= 218) {
        const size_t previous = cursor;
        nextCodepoint(reinterpret_cast<const uint8_t *>(note), strlen(note),
                      cursor);
        if (textWidth(Body, reinterpret_cast<const uint8_t *>(note) + start,
                      cursor - start) > 240 &&
            previous > start) {
          memcpy(textValue, note + start, previous - start);
          textValue[previous - start] = 0;
          text(s, 20, y, textValue, White);
          y += 29;
          start = previous;
        }
      }
      if (cursor > start && y <= 218) {
        memcpy(textValue, note + start, cursor - start);
        textValue[cursor - start] = 0;
        text(s, 20, y, textValue, White);
      }
    }
    watchPresentScene(s);
  }
};
} // namespace
Face *createCompanionFace() { return new CompanionFace; }
const FaceInfo &companionFaceInfo() { return metadata; }
