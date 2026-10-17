#include "assets/flint_fi.h"
#include "host_check.h"
#include <cstdint>

int main() {
  FiHeader h{kFiMagic, 64, 64, static_cast<uint8_t>(FiFormat::L1), 0, 0, 512};
  hostRequire(fiValidate(h, 528), "valid L1");
  h.payload_len = 0;
  hostRequire(!fiValidate(h, 16), "reject empty pixels for nonempty image");
  h.payload_len = UINT32_MAX;
  hostRequire(!fiValidate(h, 16), "reject overflowing payload");
  h = {kFiMagic, 3, 2, static_cast<uint8_t>(FiFormat::P4), kFiFlagHasTrans, 16, 4};
  hostRequire(fiValidate(h, 52), "P4 odd width has row padding");
  hostRequire(!fiValidate(h, 51), "reject truncated palette/pixels");
  h.palette_len = 17;
  hostRequire(!fiValidate(h, 54), "reject oversized P4 palette");
  h = {kFiMagic, 64, 64, static_cast<uint8_t>(FiFormat::P8), 0, 256, 4096};
  hostRequire(fiValidate(h, 4624), "valid P8");
  h.palette_len = 0;
  hostRequire(!fiValidate(h, 4112), "reject missing palette");
  h = {kFiMagic, 64, 64, static_cast<uint8_t>(FiFormat::R565), 0, 0, 8192};
  hostRequire(fiValidate(h, 8208), "valid RGB565");
  h.w = 65;
  hostRequire(!fiValidate(h, 8208), "reject excessive dimensions");
  h.w = 0;
  hostRequire(!fiValidate(h, 8208), "reject zero dimensions");
  h.w = 64;
  h.format = 255;
  hostRequire(!fiValidate(h, 8208), "reject unknown format");
  std::puts("ok test_fi_validate");
}
