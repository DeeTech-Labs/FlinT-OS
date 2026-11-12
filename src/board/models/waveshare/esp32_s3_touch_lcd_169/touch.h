#pragma once

#include "input/cst816.h"
#include "input/touch_state.h"

// Preparation only: not selected by board/device_touch.h or the device tooling.
// CST816T on the Waveshare ESP32-S3-Touch-LCD-1.69, raw portrait 240 x 280.
// Confirm the received board revision and display rotation before activation.
inline constexpr flint::input::Cst816Config FLINT_TOUCH_CONFIG{
    .port = -1, .sda = 11, .scl = 10, .reset = 13, .interrupt = 14};
// Raw portrait identity only. This is not calibration for the OS landscape canvas.
inline constexpr flint::input::TouchTransform FLINT_TOUCH_TRANSFORM{240, 280};
