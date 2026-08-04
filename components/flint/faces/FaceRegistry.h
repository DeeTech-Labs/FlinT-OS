#pragma once

#include "faces/Face.h"

namespace FaceRegistry {

bool add(FaceFactory factory);
int count();
const FaceInfo* infoAt(int index);
Face* createById(const char* id);
bool setActiveId(const char* id);
const char* activeId();
Face* active();  // owned by registry; do not delete

}  // namespace FaceRegistry

// Register all built-in faces (see faces_builtin.cpp).
void facesRegisterBuiltins();
