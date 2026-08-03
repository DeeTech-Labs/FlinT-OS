#pragma once

#include <stdint.h>

void wifiNetBegin();          // start connect, non-blocking
void wifiNetMaintain();
bool wifiNetIsConnected();
bool wifiNetWaitConnected(uint32_t timeoutMs);  // optional short wait
