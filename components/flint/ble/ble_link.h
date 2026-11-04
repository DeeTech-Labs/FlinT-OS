#pragma once
#include <stddef.h>
void bleLinkBegin();
void bleLinkMaintain();
bool bleLinkPairingCommand(const char *payload, char *output, size_t capacity);
