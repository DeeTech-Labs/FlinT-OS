#pragma once
#include <flint/runtime.h>
#include <stddef.h>
void bleLinkBegin();
const char *bleLinkState();
void bleLinkMaintain();
uint64_t bleLinkNextWake();
bool bleLinkPairingCommand(const char *payload, char *output, size_t capacity);
void bleLinkDiagnostics(flint::Values &values);
