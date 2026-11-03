#pragma once
#include <stddef.h>
#include <stdint.h>

// V1 packages are bounded data, never executable code or hardware configuration.
constexpr int IgnitSlots = 3;
struct IgnitDefinition {
  char id[25]{}, name[25]{};
  uint16_t version = 0;
  uint8_t kind = 0, layout = 0; // 0 face, 1 theme, 2 bundle; digital/blocks/quiet
  uint32_t colors[6]{}; // background, surface, accent, muted, text, selection
};
void ignitsBegin();
int ignitsCount();
const IgnitDefinition* ignitAt(int index);
const IgnitDefinition* ignitFind(const char* id);
// Atomic install/update. Existing packages remain intact on parse/storage failure.
const char* ignitInstall(const char* wire);
const char* ignitRemove(const char* id);
bool ignitParse(const char* wire, IgnitDefinition& definition);

// Theme selection is persisted separately from installed package contents.
const char* ignitThemeUse(const char* id); // empty = restore built-in palette
void ignitThemeRestore();

constexpr size_t IgnitStorageReserve = 8192;
constexpr size_t IgnitAtomicWriteBudget = 8192;
// Unified lifecycle shared by the device shell and Companion transports.
const char* ignitActivate(const char* id);
const char* ignitDelete(const char* id);

uint32_t ignitFreeRam();
constexpr uint32_t IgnitRamReserve = 8192;

const char* ignitActiveTheme();
