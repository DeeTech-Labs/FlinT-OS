/*
 * FlinT OS — ESP32-C3 + GMT028-05 (ST7789 240x320)
 *
 * Docs: docs/ARCHITECTURE.md, docs/THEMES.md
 * Layout: src/<module>/ … community face plugins in src/faces/
 *
 * Note: Arduino IDE include path is the sketch root, so use "src/..."
 * Sketch folder must be named flint_os (same as this .ino).
 */

#include "src/app/app.h"

void setup() {
  appSetup();
}

void loop() {
  appLoop();
}
