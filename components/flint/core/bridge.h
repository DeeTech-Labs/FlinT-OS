#pragma once
#include <flint/core.h>
flint::Frame coreRequest(const flint::Frame &frame, flint::Session &session);
void coreDisconnect(flint::Session &session);

// Device input adapters call these only from the application task, never from ISR/BLE callbacks.
// Drivers crossing tasks must queue their own bounded input records with the captured generation.
uint64_t coreRuntimeGeneration();
flint::Error corePostInput(flint::InputEvent event);
