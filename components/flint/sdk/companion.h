#pragma once
#include <stdint.h>
enum class CompanionMode { Note, Focus, Computer };
struct CompanionState {
  CompanionMode mode = CompanionMode::Note;
  char note[97]{};
  uint64_t focusDeadlineMs = 0;
  int cpuPercent = -1;
  int memoryPercent = -1;
};
const CompanionState& companionState();
uint32_t companionFocusRemaining();
