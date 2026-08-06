#include "faces/FaceRegistry.h"

// Factories + FaceInfo live in face_*.cpp — declare here, list once below.
Face* createDigitalFace();
const FaceInfo& digitalFaceInfo();

void facesRegisterBuiltins() {
  FaceRegistry::add(createDigitalFace, digitalFaceInfo());
  // Community / extra faces: FaceRegistry::add(createYourFace, yourFaceInfo());
}
