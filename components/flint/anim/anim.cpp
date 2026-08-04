#include "anim/anim.h"

// Instant cut — gray-wash fades caused black frames. API kept for callers.

void animBegin(AnimKind kind) {
  (void)kind;
}

bool animIsActive() {
  return false;
}

bool animTick() {
  return false;
}
