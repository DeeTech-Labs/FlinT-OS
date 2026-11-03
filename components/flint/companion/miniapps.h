#pragma once
#include <stddef.h>
#include <stdint.h>

// V1 packages are bounded data, never executable code or hardware configuration.
constexpr int MiniappSlots = 3;
struct MiniappDefinition {
  char id[25]{}, name[25]{};
  uint16_t version = 0;
  uint8_t kind = 0, layout = 0; // 0 face, 1 theme, 2 bundle; digital/blocks/quiet
  uint32_t colors[6]{}; // background, surface, accent, muted, text, selection
};
void miniappsBegin();
int miniappsCount();
const MiniappDefinition* miniappAt(int index);
const MiniappDefinition* miniappFind(const char* id);
// Atomic install/update. Existing packages remain intact on parse/storage failure.
const char* miniappInstall(const char* wire);
const char* miniappRemove(const char* id);
bool miniappParse(const char* wire, MiniappDefinition& definition);

// Theme selection is persisted separately from installed package contents.
const char* miniappThemeUse(const char* id); // empty = restore built-in palette
void miniappThemeRestore();

constexpr size_t MiniappStorageReserve = 8192;
constexpr size_t MiniappAtomicWriteBudget = 8192;
// Unified lifecycle shared by the device shell and Companion transports.
const char* miniappActivate(const char* id);
const char* miniappDelete(const char* id);

uint32_t miniappFreeRam();
constexpr uint32_t MiniappRamReserve = 8192;

const char* miniappActiveTheme();
