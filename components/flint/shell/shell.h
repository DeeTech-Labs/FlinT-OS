#pragma once

// Flat shell state machine: BOOT → FACE ⇄ PICKER / SETTINGS.
// Input today: serial monitor keys (button GPIO can call shellHandleKey later).

void shellBegin();
void shellTick();
void shellHandleKey(char c);
void shellPollSerial();  // non-blocking stdin → shellHandleKey
