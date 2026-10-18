#pragma once
void bleLinkBegin();
void bleLinkMaintain();

const char* bleLinkState();
int bleLinkLastDisconnectReason();
int bleLinkLastEncryptionStatus();
