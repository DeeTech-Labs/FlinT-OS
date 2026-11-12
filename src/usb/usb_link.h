#pragma once
#include <flint/core.h>
bool usbLinkBegin();
void usbLinkMaintain();
uint64_t usbLinkNextWake();
void usbLinkDiagnostics(flint::Values &values);
