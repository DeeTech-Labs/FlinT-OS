#pragma once

#include "faces/Face.h"

namespace FaceRegistry {

// Register factory + static FaceInfo (no heap probe). info.id must be non-empty.
bool add(FaceFactory factory, const FaceInfo& info);
int count();
const FaceInfo* infoAt(int index);  // stable pointer into registry; do not free
Face* createById(const char* id);
bool setActiveId(const char* id);
const char* activeId();
Face* active();  // owned by registry; do not delete

}  // namespace FaceRegistry

// Register all built-in faces (see faces_builtin.cpp).
void facesRegisterBuiltins();
