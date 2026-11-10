#pragma once
#include <flint/core.h>
void displayBegin();
void displayScene(flint::Core &core, const FlintRender::Scene &scene);
void displayRecovery(const char *message);
