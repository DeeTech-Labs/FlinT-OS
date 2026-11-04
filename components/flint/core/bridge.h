#pragma once
#include <flint/core.h>
flint::Frame coreRequest(const flint::Frame &frame, flint::Session &session);
void coreDisconnect(flint::Session &session);
