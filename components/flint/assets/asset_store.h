#pragma once

#include "sdk/resources.h"

#include <LovyanGFX.hpp>
#include <stdint.h>

// LittleFS asset store — mount, pack TOC, icon blit.
// Faces never touch paths; use flintDrawIcon / flintIconAvailable.

bool assetStoreBegin();
bool assetStoreReady();
uint16_t assetStorePackVersion();
uint16_t assetStoreEntryCount();

bool assetIconPresent(FlintIconId id, uint8_t sizePx);
bool assetBlitIcon(lgfx::LGFXBase& gfx, FlintIconId id, uint8_t sizePx, int cx,
                   int cy, uint16_t fg, uint16_t bg);
