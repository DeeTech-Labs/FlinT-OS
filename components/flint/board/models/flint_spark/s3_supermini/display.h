#pragma once

#include "board/device_config.h"
#include <array>
#include <cstdint>

namespace flint::board {

// Fixed ST7789 240x320 GRAM, 240x280 glass, landscape rotation 3.
static_assert(FLINT_TFT_ROTATION == 3 && FLINT_PANEL_W == 240 && FLINT_PANEL_H == 280);
static_assert(FLINT_DESIGN_W == 280 && FLINT_DESIGN_H == 240);
inline constexpr int St7789ColumnOffset = 320 - FLINT_PANEL_H - FLINT_PANEL_OFFSET_Y;
inline constexpr int St7789RowOffset = FLINT_PANEL_OFFSET_X;
// MY | MV | ML, with the existing panel's RGB/BGR selection.
inline constexpr uint8_t St7789Madctl = 0xb0 | (FLINT_PANEL_RGB_ORDER ? 0 : 0x08);
static_assert(St7789ColumnOffset >= 0 && St7789ColumnOffset + FLINT_DESIGN_W <= 320);
static_assert(St7789RowOffset >= 0 && St7789RowOffset + FLINT_DESIGN_H <= 240);

struct DisplayInitCommand {
    uint8_t command, size;
    unsigned delayMs;
    std::array<uint8_t, 14> data;
};

// Preserve the selected glass's power and gamma settings. DISPON is deliberately
// separate: the firmware fills GRAM before enabling the display.
inline constexpr DisplayInitCommand St7789Init[]{
    {0xb7, 1, 0, {0x35}},       // GCTRL
    {0xbb, 1, 0, {0x28}},       // VCOMS
    {0xc0, 1, 0, {0x0c}},       // LCMCTRL
    {0xc2, 2, 0, {0x01, 0xff}}, // VDVVRHEN
    {0xc3, 1, 0, {0x10}},       // VRHS
    {0xc4, 1, 0, {0x20}},       // VDVSET
    {0xd0, 2, 0, {0xa4, 0xa1}}, // PWCTRL1
    {0xb0, 2, 0, {0x00, 0xc0}}, // RAMCTRL
    {0xe0, 14, 0,
        {0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0e, 0x12, 0x14, 0x17}},
    {0xe1, 14, 0,
        {0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31, 0x54, 0x47, 0x0e, 0x1c, 0x17, 0x1b, 0x1e}},
    {0x11, 0, 130, {}}, // SLPOUT
    {0x38, 0, 0, {}},   // IDMOFF
};

} // namespace flint::board
