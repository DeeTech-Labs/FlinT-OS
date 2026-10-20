#pragma once
void bleLinkBegin();
void bleLinkMaintain();

const char* bleLinkState();
int bleLinkLastDisconnectReason();
int bleLinkLastEncryptionStatus();

#include <stddef.h>
// Pairing management is exposed only over the trusted serial transport.
bool bleLinkPairingCommand(const char* payload, char* json, size_t capacity);
