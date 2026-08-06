#pragma once

#include <stdint.h>

// FlinT OS icon blob (embedded in flintpack). Little-endian on-disk layout.

static constexpr uint32_t kFiMagic = 0x00314946u;  // 'FI1\0'

enum class FiFormat : uint8_t {
  L1 = 0,    // 1 bpp, MSB first, rows byte-padded; tint with fg/bg
  P4 = 1,    // 4 bpp indices + RGB565 palette
  P8 = 2,    // 8 bpp indices + RGB565 palette
  R565 = 3,  // raw RGB565 pixels (LE)
};

enum FiFlags : uint8_t {
  kFiFlagNone = 0,
  kFiFlagHasTrans = 1u << 0,  // palette index 0 is transparent (P4/P8)
};

#pragma pack(push, 1)
struct FiHeader {
  uint32_t magic;
  uint16_t w;
  uint16_t h;  // each ≤ 64
  uint8_t format;
  uint8_t flags;
  uint16_t palette_len;
  uint32_t payload_len;
};
#pragma pack(pop)

static_assert(sizeof(FiHeader) == 16, "FiHeader size");

inline size_t fiL1Stride(uint16_t w) {
  return (static_cast<size_t>(w) + 7u) / 8u;
}

inline size_t fiL1PayloadBytes(uint16_t w, uint16_t h) {
  return fiL1Stride(w) * static_cast<size_t>(h);
}
