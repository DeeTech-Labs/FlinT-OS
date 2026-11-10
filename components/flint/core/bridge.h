#pragma once
#include <flint/core.h>
flint::Frame coreRequest(const flint::Frame &frame, flint::Session &session);
void coreDisconnect(flint::Session &session);
// Allocate identities only on the Core owner task; shared by USB and BLE.
uint64_t coreNewSession();
inline constexpr uint32_t CoreWakeUsbRx = 1, CoreWakeUsbTx = 2, CoreWakeUsbSession = 4,
                          CoreWakeBle = 8, CoreWakeWork = 16;
void coreWake(uint32_t reason);
bool coreTakeCompanionEvent(const flint::Session &session, flint::Frame &event);
uint64_t coreCompanionNextWake(const flint::Session &session);

// Device input adapters call these only from the application task, never from ISR/BLE callbacks.
// Drivers crossing tasks must queue their own bounded input records with the captured generation.
uint64_t coreRuntimeGeneration();
flint::Error corePostInput(flint::InputEvent event);
