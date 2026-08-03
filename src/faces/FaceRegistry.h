#pragma once

#include "src/faces/Face.h"

namespace FaceRegistry {

bool add(FaceFactory factory);
int count();
const FaceInfo* infoAt(int index);
Face* createById(const char* id);
bool setActiveId(const char* id);
const char* activeId();
Face* active();  // owned by registry; do not delete

}  // namespace FaceRegistry

// Put in face_*.cpp after factory function:
//   FLINT_REGISTER_FACE(createMyFace);
#define FLINT_REGISTER_FACE(factoryFn)                            \
  static const bool FLINT_FACE_REG_##factoryFn = []() {           \
    FaceRegistry::add(factoryFn);                                 \
    return true;                                                  \
  }()
