#pragma once

#include <stdint.h>
#include "src/config/config.h"

// LVGL 8.x port over LovyanGFX. Faces own the panel until lvglPortSetActive(true).

#if FLINT_ENABLE_LVGL
void lvglPortBegin();
void lvglPortTick(uint32_t ms);
void lvglPortHandler();
void lvglPortSetActive(bool on);
bool lvglPortIsActive();
#else
inline void lvglPortBegin() {}
inline void lvglPortTick(uint32_t) {}
inline void lvglPortHandler() {}
inline void lvglPortSetActive(bool) {}
inline bool lvglPortIsActive() { return false; }
#endif
