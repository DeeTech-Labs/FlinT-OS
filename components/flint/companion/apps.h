#pragma once
void companionAppsBegin();
// Returns nullptr on success, or a stable error code; nullptr input is invalid.
const char* companionAppCommand(const char* operation, const char* arguments);
