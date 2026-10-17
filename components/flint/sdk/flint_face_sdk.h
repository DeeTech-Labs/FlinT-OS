#pragma once

// FlinT OS Face SDK — the only public surface community faces should include.
//
// Allowed: this header (+ C/C++ std headers).
// Forbidden: app/, settings/, usb/, shell/.
//
// Faces read FaceContext (filled by shell) and draw via display helpers / resources.

#include "faces/Face.h"
#include "faces/FaceContext.h"
#include "faces/glance.h"
#include "display/display.h"
#include "display/ui_palette.h"
#include "display/ui_tokens.h"
#include "sdk/resources.h"
#include "i18n/i18n.h"
#include "time/date_fmt.h"
#include "ui/ui_dirty.h"  // UI_DIRTY_* for onTick (SETTINGS is soft)
