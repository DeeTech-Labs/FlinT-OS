#include "companion/ignits.h"
#include "settings/cfg_fs.h"
#include "storage/package_storage.h"
#include "render/builders.h"
#include <cstdio>
#include <cstring>
namespace {
IgnitDefinition installed[IgnitSlots];
char activeTheme[25]{};
bool number(const char* s, unsigned& value, unsigned max, unsigned base = 10) {
  if (!s || !*s) return false;
  value = 0;
  for (; *s; ++s) {
    unsigned d = *s >= '0' && *s <= '9' ? *s - '0' :
                 *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : 99;
    if (d >= base || d > max || value > (max - d) / base) return false;
    value = value * base + d;
  }
  return true;
}
void pathFor(int slot, char* path, size_t n) { snprintf(path, n, "/storage/packages/ignit%d.ignit", slot); }
}
bool ignitParse(const char* wire, IgnitDefinition& out) {
  if (!wire || strlen(wire) >= 256) return false;
  char buffer[256]; strcpy(buffer, wire);
  char* parts[12]{}; char* save = nullptr; int count = 0;
  for (char* p = strtok_r(buffer, " ", &save); p; p = strtok_r(nullptr, " ", &save)) {
    if (count == 12) return false;
    parts[count++] = p;
  }
  if (count != 12 || strcmp(parts[0], "1") || strncmp(parts[1], "pkg.", 4) ||
      strlen(parts[1]) < 5 || strlen(parts[1]) > 24) return false;
  for (const char* p = parts[1]; *p; ++p)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return false;
  IgnitDefinition next;
  strcpy(next.id, parts[1]);
  const size_t nameLength = strlen(parts[2]);
  if (!nameLength || nameLength % 2 || nameLength > 48) return false;
  for (size_t i = 0; i < nameLength / 2; ++i) {
    char hex[]{parts[2][2*i], parts[2][2*i+1], 0}; unsigned byte;
    if (!number(hex, byte, 255, 16) || byte < 32 || byte == 127) return false;
    next.name[i] = char(byte);
  }
  // Strict UTF-8, no controls, overlong encodings or surrogate codepoints.
  for (size_t i = 0; i < nameLength/2;) {
    const auto first = uint8_t(next.name[i++]);
    if (first < 128) continue;
    const int extra = first >= 0xc2 && first <= 0xdf ? 1 :
                      first >= 0xe0 && first <= 0xef ? 2 :
                      first >= 0xf0 && first <= 0xf4 ? 3 : -1;
    if (extra < 0 || i + extra > nameLength/2) return false;
    uint32_t cp = first & ((1u << (6-extra)) - 1);
    for (int n = 0; n < extra; ++n) {
      auto byte = uint8_t(next.name[i++]);
      if ((byte & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (byte & 63);
    }
    if (cp < (extra == 1 ? 128u : extra == 2 ? 2048u : 65536u) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || cp < 160) return false;
  }
  unsigned version, kind, layout;
  if (!number(parts[3], version, 65535) || !version || !number(parts[4], kind, 2) ||
      !number(parts[5], layout, 2)) return false;
  next.version = version; next.kind = kind; next.layout = layout;
  for (int i = 0; i < 6; ++i) {
    unsigned color;
    if (strlen(parts[i+6]) != 6 || !number(parts[i+6], color, 0xffffff, 16)) return false;
    next.colors[i] = color;
  }
  out = next;
  return true;
}
void ignitsBegin() {
  for (auto& item : installed) item = {};
  for (int i = 0; i < IgnitSlots; ++i) {
    char path[32], text[256]; pathFor(i,path,sizeof(path));
    IgnitDefinition next;
    if (packageStorageReadFile(path,text,sizeof(text)) >= 0 && ignitParse(text,next) && !ignitFind(next.id)) installed[i] = next;
  }
}
int ignitsCount() { int n=0; for (const auto& p:installed) if (*p.id) ++n; return n; }
const IgnitDefinition* ignitAt(int index) {
  if (index < 0) return nullptr;
  for (const auto& p:installed) if (*p.id && index-- == 0) return &p;
  return nullptr;
}
const IgnitDefinition* ignitFind(const char* id) {
  if (!id) return nullptr;
  for (const auto& p:installed) if (*p.id && !strcmp(id,p.id)) return &p;
  return nullptr;
}
const char* ignitInstall(const char* wire) {
  IgnitDefinition next;
  if (!ignitParse(wire,next)) return "invalid_package";
  int slot = -1;
  for (int i=0;i<IgnitSlots;++i) if (!strcmp(installed[i].id,next.id)) { slot=i; break; }
  if (slot < 0) for(int i=0;i<IgnitSlots;++i) if (!*installed[i].id) { slot=i; break; }
  if (slot < 0) return "package_storage_full";
  if (*installed[slot].id && next.kind != installed[slot].kind) return "package_kind_changed";
  if (*installed[slot].id && next.version < installed[slot].version) return "package_downgrade";
  if(ignitFreeRam()<IgnitRamReserve)return "insufficient_ram";
  PackageStorageInfo storage;
  if(!packageStorageInfo(storage))return "storage_unavailable";
  if(storage.free < IgnitStorageReserve + IgnitAtomicWriteBudget + strlen(wire))return "insufficient_storage";
  char path[32]; pathFor(slot,path,sizeof(path));
  if (!packageStorageWriteAtomic(path,wire,strlen(wire))) return "storage_error";
  installed[slot] = next;
  return nullptr;
}
const char* ignitRemove(const char* id) {
  if(!id)return "invalid_arguments";
  if(!strcmp(id,"digital"))return "protected_package";
  for (int i=0;i<IgnitSlots;++i) if (*installed[i].id && !strcmp(installed[i].id,id)) {
    char path[32];pathFor(i,path,sizeof(path));
    if (!packageStorageRemove(path)) return "storage_error";
    installed[i]={};return nullptr;
  }
  return "unknown_package";
}

namespace {
void applyTheme(const IgnitDefinition* p) {
  FlintRender::ThemeColors colors;
  if (p) colors = {FlintRender::rgb24(p->colors[0]), FlintRender::rgb24(p->colors[1]),
      FlintRender::rgb24(p->colors[2]), FlintRender::rgb24(p->colors[3]),
      FlintRender::rgb24(p->colors[4]), FlintRender::rgb24(p->colors[5])};
  FlintRender::setTheme(colors);
}
}
const char* ignitThemeUse(const char* id) {
  if (!id) return "invalid_arguments";
  const auto* p = *id ? ignitFind(id) : nullptr;
  if (*id && (!p || p->kind == 0)) return "unknown_theme";
  if (!cfgFsWriteAtomic("/cfg/theme.cfg",id,strlen(id))) return "storage_error";
  strcpy(activeTheme,id);applyTheme(p);return nullptr;
}
void ignitThemeRestore() {
  char id[25]{};
  cfgFsReadFile("/cfg/theme.cfg",id,sizeof(id));
  const auto* p = ignitFind(id);
  const bool valid=p && p->kind!=0;
  strcpy(activeTheme,valid?id:"");
  applyTheme(valid?p:nullptr);
}

const char* ignitActiveTheme(){return activeTheme;}
