#pragma once

#include <stdint.h>
#include "config/flint_version.h"
#include "faces/FaceContext.h"

// Community face contract (see docs/ARCHITECTURE.md + sdk/flint_face_sdk.h).
// Implement Face, export createXxxFace(), register in faces_builtin.cpp.
// Do not call USB / GPIO / NVS from a face — only read FaceContext and draw.

struct FaceInfo {
  const char* id;       // unique, e.g. "digital"
  const char* name;     // human title
  const char* author;   // developer org, e.g. "DeeTech Labs"
  uint16_t version;     // theme version
  uint16_t minApi;      // must be <= FLINT_OS_API_VERSION (flint_version.h)
};

class Face {
public:
  virtual ~Face() {}

  virtual const FaceInfo& info() const = 0;

  // Called when face becomes active
  virtual void onActivate(const FaceContext& ctx) { (void)ctx; }

  virtual void onDeactivate() {}

  // Full canvas repaint: day-mode change, first show, TIME_VALID, explicit FORCE.
  virtual void onForceRedraw(const FaceContext& ctx) = 0;

  // Incremental update when dirty events fire. UI_DIRTY_SETTINGS is soft: prefs
  // in FaceContext (and i18n) may have changed — rebuild layout as needed
  // (often a full paint) without waiting for FORCE.
  virtual void onTick(const FaceContext& ctx) = 0;
};

using FaceFactory = Face* (*)();
