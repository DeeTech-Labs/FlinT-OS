#pragma once

#include "assets/flint_fi.h"
#include "config/flint_version.h"

#include <stdint.h>

// FlinT OS asset pack on LittleFS (`/storage/core.flintpack`).

static constexpr uint32_t kFlintPackMagic = 0x4B504C46u;  // 'FLPK'
static constexpr uint16_t kFlintPackFormatVer = FLINT_PACK_FORMAT_VER;
static constexpr const char* kAssetMountPoint = "/storage";
static constexpr const char* kAssetPackPath = "/storage/core.flintpack";

#pragma pack(push, 1)
struct FlintPackHeader {
  uint32_t magic;
  uint16_t format_ver;
  uint16_t api_min;
  uint16_t pack_ver;
  uint16_t entry_count;
  uint32_t reserved;
};

struct FlintPackEntry {
  uint16_t icon_id;
  uint8_t size_px;
  uint8_t format;  // FiFormat
  uint32_t offset;  // absolute file offset of FiHeader
  uint32_t length;  // FiHeader + palette + payload
};
#pragma pack(pop)

static_assert(sizeof(FlintPackHeader) == 16, "FlintPackHeader size");
static_assert(sizeof(FlintPackEntry) == 12, "FlintPackEntry size");
