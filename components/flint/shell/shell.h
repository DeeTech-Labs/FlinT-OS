#pragma once

// Flat shell state machine: BOOT → FACE ⇄ PICKER / SETTINGS.
// Input today: serial monitor keys (button GPIO can call shellHandleKey later).

void shellBegin();
void shellTick();
void shellHandleKey(char c);
void shellPollSerial();  // non-blocking input → shellHandleKey
// Block until input or the next service deadline, then dispatch received keys.
void shellWaitForInput(unsigned timeoutMs);
