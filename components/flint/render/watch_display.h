#pragma once
#include "scene.h"

void watchPresentScene(const FlintRender::Scene &scene);
void watchDisplayPresented();
void watchFadeInNextScene();
void watchDisplayMaintain();
// Read-only state/control operations; caller enforces the transport trust
// policy.
bool watchDisplayCommand(const char *operation, const char *arguments,
                         char *json, size_t capacity);
