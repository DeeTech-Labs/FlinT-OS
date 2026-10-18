#include "faces/FaceRegistry.h"

// Factories + FaceInfo live in face_*.cpp — declare here, list once below.
Face* createCompanionFace();
const FaceInfo& companionFaceInfo();
Face* createStudioFace();
const FaceInfo& studioFaceInfo();
Face* createDigitalFace();
const FaceInfo& digitalFaceInfo();

void facesRegisterBuiltins() {
  FaceRegistry::add(createDigitalFace, digitalFaceInfo());
  FaceRegistry::add(createStudioFace, studioFaceInfo());
  FaceRegistry::add(createCompanionFace, companionFaceInfo());
  // Community / extra faces: FaceRegistry::add(createYourFace, yourFaceInfo());
}
