#pragma once

#include <stdint.h>
#include <stddef.h>

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

// Validate the complete blob before any decoder reads pixels or palette entries.
inline bool fiValidate(const FiHeader& hdr, size_t blobBytes) {
  if (hdr.magic != kFiMagic || hdr.w == 0 || hdr.h == 0 ||
      hdr.w > 64 || hdr.h > 64 || (hdr.flags & ~kFiFlagHasTrans)) {
    return false;
  }
  size_t expected = 0;
  switch (static_cast<FiFormat>(hdr.format)) {
    case FiFormat::L1:
      if (hdr.palette_len != 0 || hdr.flags != 0) return false;
      expected = fiL1PayloadBytes(hdr.w, hdr.h);
      break;
    case FiFormat::P4:
      if (hdr.palette_len == 0 || hdr.palette_len > 16) return false;
      expected = ((static_cast<size_t>(hdr.w) + 1) / 2) * hdr.h;
      break;
    case FiFormat::P8:
      if (hdr.palette_len == 0 || hdr.palette_len > 256) return false;
      expected = static_cast<size_t>(hdr.w) * hdr.h;
      break;
    case FiFormat::R565:
      if (hdr.palette_len != 0 || hdr.flags != 0) return false;
      expected = static_cast<size_t>(hdr.w) * hdr.h * 2;
      break;
    default:
      return false;
  }
  return hdr.payload_len == expected &&
         sizeof(FiHeader) + static_cast<size_t>(hdr.palette_len) * 2 + expected == blobBytes;
}
