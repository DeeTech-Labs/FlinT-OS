#include "companion/studio.h"
#include "sdk/studio.h"
#include "host_check.h"
#include <string>
#include <cstring>
static bool storageOkay = true;
static std::string stored;
bool cfgFsWriteAtomic(const char*,const void* data,size_t length) { if (!storageOkay) return false; stored.assign(static_cast<const char*>(data),length); return true; }
int cfgFsReadFile(const char*,char* data,size_t size) { if (stored.size()+1 > size || stored.empty()) return -1; strcpy(data,stored.c_str()); return stored.size(); }
int main() {
  const std::string pixels(64,'f');
  const auto valid = std::string("101112 d4fa70 128 180 1 ") + pixels;
  hostRequire(studioInstall(valid.c_str()),"install bounded studio definition");
  hostRequire(studioDefinition().pixels[0] == 255 && studioDefinition().timeY == 128,"decode pixel bitmap");
  for (const auto& invalid : {std::string("zzzzzz d4fa70 128 180 1 ")+pixels,
      std::string("101112 d4fa70 999999999999999999999 180 1 ")+pixels,
      std::string("101112 d4fa70 79 180 1 ")+pixels,
      std::string("101112 d4fa70 128 180 2 ")+pixels,valid+" trailing",valid.substr(0,valid.size()-1)})
    hostRequire(!studioInstall(invalid.c_str()),"reject malformed definition");
  storageOkay = false;
  const auto different = std::string("ffffff 000000 100 190 0 ")+std::string(64,'0');
  hostRequire(!studioInstall(different.c_str()) && studioDefinition().background == 0x101112,"failed save preserves active design");
  studioBegin();
  hostRequire(studioDefinition().pixels[31] == 255,"reload saved design");
}
