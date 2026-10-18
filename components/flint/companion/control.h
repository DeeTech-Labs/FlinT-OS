#pragma once
#include "usb/pc_protocol.h"
void companionExecute(const PcCommand& command, char* response, size_t capacity, bool trustedTransport = false);
