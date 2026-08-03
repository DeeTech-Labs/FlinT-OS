#pragma once

#include <stdint.h>
#include "src/faces/FaceContext.h"

struct FaceInfo {
  const char* id;       // unique, e.g. "digital"
  const char* name;     // human title
  const char* author;
  uint16_t version;     // theme version
  uint16_t minApi;      // min FLINT_OS_API_VERSION
};

// Community face contract — implement this, register, done.
class Face {
public:
  virtual ~Face() {}

  virtual const FaceInfo& info() const = 0;

  // Called when face becomes active
  virtual void onActivate(const FaceContext& ctx) { (void)ctx; }

  virtual void onDeactivate() {}

  // Full repaint (mode change, first show, explicit force)
  virtual void onForceRedraw(const FaceContext& ctx) = 0;

  // Periodic update — redraw only dirty parts
  virtual void onTick(const FaceContext& ctx) = 0;
};

using FaceFactory = Face* (*)();
