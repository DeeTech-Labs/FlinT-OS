#pragma once

#include <stdint.h>

void wifiNetBegin();          // start connect, non-blocking
void wifiNetMaintain();
bool wifiNetIsConnected();
bool wifiNetWaitConnected(uint32_t timeoutMs);  // optional short wait

// STA RSSI while associated; INT8_MIN when disconnected / unknown.
// Prefer the maintain()/link-event cache (no ioctl on the FaceContext path).
int8_t wifiNetRssi();
// Discrete UI bars: 0 = off, 1–3 = signal strength from RSSI.
// Cached — refreshed by wifiNetMaintain / GOT_IP / disconnect (not per paint).
uint8_t wifiNetBars();
