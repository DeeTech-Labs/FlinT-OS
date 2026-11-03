#include "faces/FaceRegistry.h"
Face* createDigitalFace();
const FaceInfo& digitalFaceInfo();
void facesRegisterBuiltins() {
  FaceRegistry::add(createDigitalFace, digitalFaceInfo());
}
