#pragma once

// FlinT OS — LovyanGFX device for the selected board profile.
// Do not add per-panel #includes here: gen_board_catalog.py writes
// board/board_impl.inc → board/impl/<id>.h (authoring stays under board/).

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "board/board_config.inc"
#include "board/board_impl.inc"
