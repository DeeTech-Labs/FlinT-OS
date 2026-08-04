#include "faces/FaceRegistry.h"

// Factories live in face_*.cpp — declare here, list once below.
Face* createDigitalFace();

void facesRegisterBuiltins() {
  FaceRegistry::add(createDigitalFace);
  // Community / extra faces: FaceRegistry::add(createYourFace);
}
