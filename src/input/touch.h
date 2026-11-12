#pragma once
#include <cstdint>

// Core-side queue drain; I2C belongs exclusively to the hardware worker.
bool touchBegin();
bool touchReady();
void touchMaintain();
void touchInterrupt();
uint64_t touchNextWake();

// Only present when the selected board configures a touch controller.
bool touchWorkerBegin();
void touchWorkerMaintain(uint64_t at);
void touchWorkerInterrupt();
uint64_t touchWorkerNextWake();
