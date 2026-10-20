#pragma once
void studioBegin();
bool studioInstall(const char* definition);

#include <stdint.h>
// Called by the app task; hidden/inactive faces never drive animation redraws.
bool studioAnimate(uint32_t now, bool active);
