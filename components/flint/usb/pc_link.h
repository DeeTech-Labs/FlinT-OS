#pragma once

void pcLinkBegin();
void pcLinkMaintain();
bool pcLinkConnected();
// Returns true when the byte belongs to the PC protocol, not shell navigation.
bool pcLinkHandleByte(char c);
