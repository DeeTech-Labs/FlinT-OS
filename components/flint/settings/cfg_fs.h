#pragma once

#include <stddef.h>
#include <stdint.h>

// LittleFS partition "cfg" mounted at /cfg — UI prefs + soft clock (runtime R/W).

static constexpr const char* kCfgMountPoint = "/cfg";
static constexpr const char* kCfgPartitionLabel = "cfg";
static constexpr const char* kCfgUiPath = "/cfg/ui.cfg";
static constexpr const char* kCfgTimePath = "/cfg/time.cfg";

bool cfgFsBegin();
bool cfgFsReady();

// Write bytes atomically: path.tmp → fflush/fsync → rename to path.
bool cfgFsWriteAtomic(const char* path, const void* data, size_t len);

// Read whole file into buf (NUL-terminated). Returns bytes read, or -1 on
// I/O failure or insufficient space; partial contents are not returned.
int cfgFsReadFile(const char* path, char* buf, size_t bufSize);
