#include "render/boot.h"
#include "render/watch_faces.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace FlintRender;
static void checkFontIndex() {
  // Independent VLW scan validates every generated bitmap offset and metric.
  for (unsigned font = Body; font <= Brand; ++font) {
    const auto* data = fontData(font);
    const int count = be32(data);
    size_t offset = 24 + count * 28;
    uint32_t previous = 0;
    for (int i = 0; i < count; ++i) {
      const auto* entry = data + 24 + i * 28;
      const auto code = uint32_t(be32(entry));
      assert(i == 0 || code > previous);
      const auto actual = glyph(font, code);
      assert(actual.alpha == data + offset);
      assert(actual.width == be32(entry + 8));
      assert(actual.height == be32(entry + 4));
      assert(actual.advance == be32(entry + 12));
      offset += actual.width * actual.height;
      previous = code;
    }
    assert(glyph(font, 0x10ffff).alpha == glyph(Body, '?').alpha);
  }
}
static void checkClockLayouts() {
  std::vector<uint16_t> incremental(280 * 240), full(280 * 240);
  for (auto style : {ClockStyle::Digital, ClockStyle::Blocks, ClockStyle::Quiet}) {
    ClockText textValue{"23:59", "23", "59", "09", "Понедельник, 31 декабря",
                        "USB", "PM", true};
    auto previous = clockScene(style, textValue);
    Canvas initial(incremental.data(), 280, 240); render(previous, initial);
    for (int step = 0; step < 5; ++step) {
      if (step == 0) textValue.seconds = "10";
      if (step == 1) { textValue.time = "00:00"; textValue.hours = "00";
                      textValue.minutes = "00"; textValue.date = "1 января"; }
      if (step == 2) textValue.showSeconds = false;
      if (step == 3) { textValue.time = "--:--"; textValue.hours = "--";
                      textValue.minutes = "--"; textValue.date = "Ожидание времени";
                      textValue.status = ""; textValue.meridiem = ""; }
      const auto scene = clockScene(style, textValue);
      assert(scene.valid);
      uint8_t wire[MaxEncoded]; Scene decoded;
      assert(decode(wire, encode(scene, wire, sizeof(wire)), decoded));
      const auto area = dirty(previous, scene);
      if (step == 0) {
        assert(area.y >= 200);
        assert(area.w * area.h < 1200); // seconds never repaint the clock/date
      }
      if (step == 4) assert(area.w == 0 && area.h == 0);
      Canvas delta(incremental.data(), 280, 240); delta.clip = area;
      render(scene, delta);
      Canvas reference(full.data(), 280, 240); render(scene, reference);
      assert(incremental == full);
      previous = scene;
    }
  }
  // Three two-line rows fit between header and footer, including long Cyrillic.
  auto previous = begin("Дата и время");
  for (int selected = 0; selected < VisibleRows; ++selected) {
    auto scene = begin("Дата и время"); counter(scene, selected, 6);
    for (int rowIndex = 0; rowIndex < VisibleRows; ++rowIndex)
      row(scene, rowIndex, "Длинное название настройки часового пояса",
          "Москва, Санкт-Петербург, Волгоград", rowIndex == selected);
    footer(scene, "p/n · +/- · Esc");
    assert(scene.valid);
    Canvas delta(incremental.data(),280,240);delta.clip=dirty(previous,scene);
    render(scene,delta);
    Canvas reference(full.data(),280,240);render(scene,reference);
    assert(incremental == full);
    previous = scene;
  }
}
int main() {
  checkFontIndex();
  checkClockLayouts();
  Scene previous = begin("FlinT Spark");
  text(previous,140,130,"23:59",White,TimeLarge,true);
  text(previous,140,184,"Среда, 31 декабря",Muted,Body,true);
  std::vector<uint16_t> incremental(280*240), full(280*240), swapped(280*240);
  Canvas initial(incremental.data(),280,240); render(previous,initial);
  for(int n=0;n<120;++n) {
    Scene scene = begin(n<60 ? "FlinT Spark" : "Настройки");
    char time[24]; snprintf(time,sizeof(time),"%02d:%02d",n/60,n%60);
    text(scene,140,130,time,White,n%3 ? TimeLarge : TimeCompact,true);
    text(scene,140,184,n%2 ? "Вторник, 20 октября" : "Среда, 31 декабря",Muted,Body,true);
    rect(scene,n-20,195,40,25,Accent,8);
    Canvas delta(incremental.data(),280,240); delta.clip=dirty(previous,scene);render(scene,delta);
    Canvas reference(full.data(),280,240);render(scene,reference);
    assert(incremental==full);
    Canvas other(swapped.data(),280,240,true); render(scene,other);
    assert(pixelHash(reference)==pixelHash(other));
    uint8_t bytes[MaxEncoded];const auto size=encode(scene,bytes,sizeof(bytes));assert(size);
    Scene decoded;assert(decode(bytes,size,decoded));
    for(size_t truncated=0;truncated<size;++truncated) assert(!decode(bytes,truncated,decoded));
    bytes[2]=99;assert(!decode(bytes,size,decoded));
    previous=scene;
  }
  previous = bootScene(0); Canvas bootInitial(incremental.data(),280,240); render(previous,bootInitial);
  for(unsigned t=40;t<=BootDurationMs+40;t+=40) {
    const auto scene=bootScene(t); uint8_t wire[MaxEncoded]; Scene decoded;
    assert(scene.valid); const auto size=encode(scene,wire,sizeof(wire)); assert(decode(wire,size,decoded));
    Canvas delta(incremental.data(),280,240);delta.clip=dirty(previous,scene);render(scene,delta);
    Canvas reference(full.data(),280,240);render(scene,reference);assert(incremental==full);previous=scene;
  }
  for(const auto pixel:full) assert(pixel==Background);
  Scene overflow;for(int i=0;i<MaxOps+1;++i) rect(overflow,0,0,1,1,0);assert(!overflow.valid);
  puts("renderer: 120 dirty/full, wire round trips, truncation and byte-order checks passed");
}
