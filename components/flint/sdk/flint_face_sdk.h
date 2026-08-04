#pragma once

// FlinT Face SDK — the only public surface community faces should include.
//
// Allowed: this header (+ C/C++ std headers).
// Forbidden: net/, weather/*.cpp APIs, app/, settings/, config/secrets.h, shell/.
//
// Faces read FaceContext (filled by shell) and draw via display helpers / resources.

#include "faces/Face.h"
#include "faces/FaceContext.h"
#include "display/display.h"
#include "display/ui_palette.h"
#include "sdk/resources.h"
