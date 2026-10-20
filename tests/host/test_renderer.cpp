#include "render/boot.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace FlintRender;
int main() {
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
