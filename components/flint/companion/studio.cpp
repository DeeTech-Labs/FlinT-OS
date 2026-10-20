#include "companion/studio.h"
#include "sdk/studio.h"
#include "settings/cfg_fs.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <initializer_list>
namespace {
StudioDefinition definition;
bool animationFrame = false;
uint32_t lastAnimation = 0;
bool parse(const char* text, StudioDefinition& next) {
  if (!text || strlen(text) >= 224) return false;
  char buffer[224]; strcpy(buffer,text);
  char* fields[13]{}; char* save = nullptr; unsigned count = 0;
  for (char* token = strtok_r(buffer," ",&save); token; token = strtok_r(nullptr," ",&save)) {
    if (count >= 13) return false;
    fields[count++] = token;
  }
  if ((count != 6 && count != 13) || strlen(fields[0]) != 6 || strlen(fields[1]) != 6 || strlen(fields[5]) != 64) return false;
  for (int index : {0,1,5}) for (const char* p = fields[index]; *p; ++p) if (!isxdigit(static_cast<unsigned char>(*p))) return false;
  int numbers[3]{};
  for (int index = 0; index < 3; ++index) {
    for (const char* p = fields[index+2]; *p; ++p) {
      if (*p < '0' || *p > '9') return false;
      numbers[index] = numbers[index] * 10 + *p - '0';
      if (numbers[index] > 220) return false;
    }
  }
  if (numbers[0] < 80 || numbers[0] > 150 || numbers[1] < 170 || numbers[1] > 220 || numbers[2] > 1) return false;
  next.background = strtoul(fields[0],nullptr,16); next.foreground = strtoul(fields[1],nullptr,16);
  next.timeY = numbers[0]; next.dateY = numbers[1]; next.showDate = numbers[2];
  for (int i = 0; i < 32; ++i) { char byte[3]{fields[5][i*2],fields[5][i*2+1],0}; next.pixels[i] = strtoul(byte,nullptr,16); }
  if (count == 13) {
    int extra[6]{};
    for (int i = 0; i < 6; ++i) {
      if (!*fields[i + 6]) return false;
      for (const char* p = fields[i + 6]; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        extra[i] = extra[i] * 10 + *p - '0';
        if (extra[i] > 280) return false;
      }
    }
    if (extra[2] < 1 || extra[2] > 6 || extra[0] + 16 * extra[2] > 280 || extra[1] + 16 * extra[2] > 240
        || extra[3] > 1 || extra[4] > 5 || extra[5] > 1 || strlen(fields[12]) != 64) return false;
    for (const char* p = fields[12]; *p; ++p) if (!isxdigit(static_cast<unsigned char>(*p))) return false;
    next.iconX = extra[0]; next.iconY = extra[1]; next.iconScale = extra[2]; next.showIcon = extra[3]; next.layerOrder = extra[4]; next.animateIcon = extra[5];
    for (int i = 0; i < 32; ++i) { char byte[3]{fields[12][i*2],fields[12][i*2+1],0}; next.alternatePixels[i] = strtoul(byte,nullptr,16); }
  }
  return true;
}
}
const StudioDefinition& studioDefinition() { return definition; }
void studioBegin() {
  char buffer[224]; StudioDefinition next;
  if (cfgFsReadFile("/cfg/studio.cfg",buffer,sizeof(buffer)) >= 0 && parse(buffer,next)) definition = next;
}
bool studioInstall(const char* text) {
  StudioDefinition next;
  if (!parse(text,next) || !cfgFsWriteAtomic("/cfg/studio.cfg",text,strlen(text))) return false;
  definition = next; return true;
}

bool studioAnimationFrame() { return animationFrame; }
bool studioAnimate(uint32_t now, bool active) {
  if (!active || !definition.animateIcon || !definition.showIcon) { animationFrame = false; lastAnimation = now; return false; }
  if (uint32_t(now - lastAnimation) < 500) return false;
  lastAnimation = now; animationFrame = !animationFrame; return true;
}
