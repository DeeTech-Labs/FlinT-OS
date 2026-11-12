#pragma once
#include <cstdint>

// All functions run on the Core owner task. The hardware ISR only wakes that task.
bool touchBegin();
bool touchReady();
void touchMaintain();
void touchInterrupt();
uint64_t touchNextWake();
