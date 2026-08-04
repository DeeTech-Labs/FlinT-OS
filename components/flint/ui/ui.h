#pragma once

#include <stdint.h>
#include "faces/FaceContext.h"

// Face rendering helper. Shell owns navigation; this module paints the active face.

void uiFaceBegin(const char* preferredFaceId);  // register + activate preferred/fallback
void uiFaceActivateCurrent();                   // onActivate for active face
void uiForceRedraw();
void uiFaceTick();                              // paints only when dirty flags say so

FaceContext uiBuildContext();
bool uiWantsSeconds();
