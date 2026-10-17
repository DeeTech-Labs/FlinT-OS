#pragma once

#include <stdint.h>
#include "config/flint_version.h"
#include "board/board_config.inc"

// FlinT OS — shared configuration (safe to commit)
// Versions: config/flint_version.h
// Display pins / panel: components/flint/board/ (see board_config.inc)

inline constexpr const char* DEFAULT_TZ_ID = "Europe/Moscow";

// Display pins / rotation (from selected FLINT_BOARD)
static const int TFT_CS = FLINT_TFT_CS;
static const int TFT_DC = FLINT_TFT_DC;
static const int TFT_RST = FLINT_TFT_RST;
static const int TFT_MOSI = FLINT_TFT_MOSI;
static const int TFT_SCLK = FLINT_TFT_SCLK;
static const uint8_t TFT_ROTATION = FLINT_TFT_ROTATION;

// Serial debug: 0=off, 1=info, 2=verbose
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1
#endif

static const uint32_t DEBUG_HEARTBEAT_MS = 10000UL;


// Factory default when ui.cfg has no active_face yet (picker / settings override).
inline constexpr const char* ACTIVE_FACE_ID = "digital";
