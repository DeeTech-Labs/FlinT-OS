#pragma once

void pcLinkBegin();
void pcLinkMaintain();
bool pcLinkConnected();
// Returns true when the byte belongs to the PC protocol, not shell navigation.
bool pcLinkHandleByte(char c);

#include "usb/pc_protocol.h"
// Execute on the app task for both USB and BLE; no transport-specific output.
void pcLinkExecute(const PcCommand& command, char* response, size_t capacity, bool trustedTransport = false);

bool pcLinkFind();
