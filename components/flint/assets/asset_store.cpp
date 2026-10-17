#include "assets/asset_store.h"
#include "assets/flint_pack.h"
#include "config/config.h"
#include "config/debug.h"
#include "util/flint_sys.h"

#include "esp_err.h"
#include "esp_littlefs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace {

// Max FI edge (matches FiHeader contract). Blit line/image budget scales with this.
constexpr size_t kMaxIconPx = 64;
// Worst-case single FI when streaming: header + 256-entry palette + R565 payload.
constexpr size_t kMaxFiBytes =
    sizeof(FiHeader) + 256u * sizeof(uint16_t) + kMaxIconPx * kMaxIconPx * 2u;
// Resident pack grows with icons; cap leaves headroom for FB on ESP32-C3.
constexpr size_t kMaxResidentPackBytes = 96u * 1024u;
constexpr size_t kMaxPackEntries = 192;

bool gReady = false;
bool gResident = false;
uint16_t gPackVer = 0;
uint16_t gEntryCount = 0;
FILE* gPackFile = nullptr;
uint8_t* gPackBytes = nullptr;
size_t gPackSize = 0;
const FlintPackEntry* gEntries = nullptr;
FlintPackEntry* gEntriesOwned = nullptr;  // stream-mode TOC
uint8_t* gFiScratch = nullptr;            // stream-mode one-FI buffer

alignas(4) uint16_t gBlitRgb[kMaxIconPx * kMaxIconPx];


void freePackMemory() {
  free(gPackBytes);
  gPackBytes = nullptr;
  gPackSize = 0;
  free(gEntriesOwned);
  gEntriesOwned = nullptr;
  free(gFiScratch);
  gFiScratch = nullptr;
  gEntries = nullptr;
  gResident = false;
  if (gPackFile) {
    fclose(gPackFile);
    gPackFile = nullptr;
  }
}

const FlintPackEntry* findEntry(uint16_t iconId, uint8_t sizePx) {
  const FlintPackEntry* fallback = nullptr;
  for (uint16_t i = 0; i < gEntryCount; i++) {
    const FlintPackEntry& e = gEntries[i];
    if (e.icon_id != iconId) {
      continue;
    }
    if (e.size_px == sizePx) {
      return &e;
    }
    if (!fallback || e.size_px < fallback->size_px) {
      fallback = &e;
    }
  }
  return fallback;
}

bool readExact(FILE* f, void* dst, size_t n) {
  return fread(dst, 1, n, f) == n;
}

bool validateEntries(size_t packSize) {
  for (uint16_t i = 0; i < gEntryCount; i++) {
    const FlintPackEntry& e = gEntries[i];
    if (e.length < sizeof(FiHeader) || e.length > kMaxFiBytes) {
      LOGE("assets", "entry %u bad length %u", (unsigned)i, (unsigned)e.length);
      return false;
    }
    if (e.offset < sizeof(FlintPackHeader) + sizeof(FlintPackEntry) * gEntryCount ||
        e.offset > packSize || e.length > packSize - e.offset) {
      LOGE("assets", "entry %u past EOF", (unsigned)i);
      return false;
    }
  }
  return true;
}

bool loadPack() {
  freePackMemory();

  FILE* f = fopen(kAssetPackPath, "rb");
  if (!f) {
    LOGE("assets", "open %s failed", kAssetPackPath);
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  const long fileSizeLong = ftell(f);
  if (fileSizeLong < static_cast<long>(sizeof(FlintPackHeader)) || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  const size_t fileSize = static_cast<size_t>(fileSizeLong);

  FlintPackHeader hdr{};
  if (!readExact(f, &hdr, sizeof(hdr))) {
    LOGE("assets", "short pack header");
    fclose(f);
    return false;
  }
  if (hdr.magic != kFlintPackMagic || hdr.format_ver != kFlintPackFormatVer) {
    LOGE("assets", "bad pack magic/ver magic=0x%08lx ver=%u",
         (unsigned long)hdr.magic, (unsigned)hdr.format_ver);
    fclose(f);
    return false;
  }
  if (hdr.api_min > FLINT_OS_API_VERSION) {
    LOGE("assets", "pack api_min=%u > firmware api=%u", (unsigned)hdr.api_min,
         (unsigned)FLINT_OS_API_VERSION);
    fclose(f);
    return false;
  }
  if (hdr.entry_count == 0 || hdr.entry_count > kMaxPackEntries) {
    LOGE("assets", "invalid entry_count=%u (max %u)", (unsigned)hdr.entry_count,
         (unsigned)kMaxPackEntries);
    fclose(f);
    return false;
  }

  const size_t tocBytes = sizeof(FlintPackEntry) * hdr.entry_count;
  if (sizeof(FlintPackHeader) + tocBytes > fileSize) {
    LOGE("assets", "TOC past EOF");
    fclose(f);
    return false;
  }

  gEntryCount = hdr.entry_count;
  gPackVer = hdr.pack_ver;

  // Prefer resident whenever pack fits the hard cap — do not gate on heap/4.
  // Stream only if oversized or malloc fails (log clearly either way).
  const bool withinCap = fileSize <= kMaxResidentPackBytes;
  if (withinCap) {
    gPackBytes = static_cast<uint8_t*>(malloc(fileSize));
    if (!gPackBytes) {
      LOGE("assets", "malloc pack %u failed (cap=%u heap=%u) — streaming",
           (unsigned)fileSize, (unsigned)kMaxResidentPackBytes,
           (unsigned)flintFreeHeap());
    } else {
      if (fseek(f, 0, SEEK_SET) != 0 || !readExact(f, gPackBytes, fileSize)) {
        LOGE("assets", "read pack body failed");
        free(gPackBytes);
        gPackBytes = nullptr;
        fclose(f);
        return false;
      }
      fclose(f);
      f = nullptr;
      gPackSize = fileSize;
      gEntries = reinterpret_cast<const FlintPackEntry*>(gPackBytes + sizeof(FlintPackHeader));
      gResident = true;
      if (!validateEntries(gPackSize)) {
        freePackMemory();
        return false;
      }
      LOGI("assets", "pack resident ver=%u api_min=%u entries=%u bytes=%u",
           (unsigned)gPackVer, (unsigned)hdr.api_min, (unsigned)gEntryCount,
           (unsigned)gPackSize);
      return true;
    }
  } else {
    LOGI("assets", "pack %u > resident cap %u — streaming", (unsigned)fileSize,
         (unsigned)kMaxResidentPackBytes);
  }

  // Stream mode: TOC in RAM, FI loaded on demand.
  gEntriesOwned = static_cast<FlintPackEntry*>(malloc(tocBytes));
  gFiScratch = static_cast<uint8_t*>(malloc(kMaxFiBytes));
  if (!gEntriesOwned || !gFiScratch) {
    LOGE("assets", "malloc stream buffers failed");
    freePackMemory();
    if (f) {
      fclose(f);
    }
    return false;
  }
  if (!readExact(f, gEntriesOwned, tocBytes)) {
    LOGE("assets", "short pack TOC");
    freePackMemory();
    fclose(f);
    return false;
  }
  gEntries = gEntriesOwned;
  gPackFile = f;
  gPackSize = fileSize;
  gResident = false;
  if (!validateEntries(gPackSize)) {
    freePackMemory();
    return false;
  }
  LOGI("assets", "pack stream ver=%u api_min=%u entries=%u bytes=%u heap=%u",
       (unsigned)gPackVer, (unsigned)hdr.api_min, (unsigned)gEntryCount, (unsigned)gPackSize,
       (unsigned)flintFreeHeap());
  return true;
}

bool loadFi(const FlintPackEntry& e, FiHeader* outHdr, const uint16_t** outPal,
            const uint8_t** outPix) {
  const uint8_t* blob = nullptr;
  if (gResident) {
    blob = gPackBytes + e.offset;
  } else {
    if (!gPackFile || !gFiScratch || e.length > kMaxFiBytes) {
      return false;
    }
    if (fseek(gPackFile, static_cast<long>(e.offset), SEEK_SET) != 0) {
      return false;
    }
    if (!readExact(gPackFile, gFiScratch, e.length)) {
      return false;
    }
    blob = gFiScratch;
  }

  memcpy(outHdr, blob, sizeof(FiHeader));
  if (!fiValidate(*outHdr, e.length)) {
    return false;
  }

  size_t cursor = sizeof(FiHeader);
  *outPal = nullptr;
  if (outHdr->palette_len > 0) {
    const size_t palBytes = static_cast<size_t>(outHdr->palette_len) * 2u;
    if (cursor + palBytes > e.length) {
      return false;
    }
    *outPal = reinterpret_cast<const uint16_t*>(blob + cursor);
    cursor += palBytes;
  }
  if (cursor + outHdr->payload_len > e.length) {
    return false;
  }
  *outPix = blob + cursor;
  return true;
}

void fiBlitOrigin(const FiHeader& hdr, int cx, int cy, int* x0, int* y0) {
  *x0 = cx - static_cast<int>(hdr.w) / 2;
  *y0 = cy - static_cast<int>(hdr.h) / 2;
}

// Chroma key for transparent pushImage — must not equal ink (fg).
uint16_t fiTransKey(uint16_t fg) {
  uint16_t key = 0xF81F;  // magenta
  if (key == fg) {
    key = 0x07E0;  // lime
  }
  return key;
}

uint16_t fiResolvePalPixel(uint8_t idx, uint16_t paletteLen, const uint16_t* pal, bool trans,
                           uint16_t transKey) {
  if (trans && idx == 0) {
    return transKey;
  }
  if (idx < paletteLen) {
    return pal[idx];
  }
  return transKey;
}

uint16_t fiPalTransKey(const uint16_t* pal, uint16_t paletteLen, uint16_t tip) {
  uint16_t key = fiTransKey(tip);
  if (!pal) {
    return key;
  }
  for (uint16_t n = 0; n < 8; n++) {
    bool clash = false;
    for (uint16_t i = 0; i < paletteLen; i++) {
      if (pal[i] == key) {
        clash = true;
        break;
      }
    }
    if (!clash) {
      return key;
    }
    key = static_cast<uint16_t>(key ^ (0x0821u << (n % 3)));
  }
  return key;
}

void blitL1(lgfx::LGFXBase& gfx, const FiHeader& hdr, const uint8_t* pix, int cx,
            int cy, uint16_t fg, uint16_t /*bg*/, uint16_t* outKey) {
  int x0 = 0;
  int y0 = 0;
  fiBlitOrigin(hdr, cx, cy, &x0, &y0);
  const uint16_t key = fiTransKey(fg);
  if (outKey) {
    *outKey = key;
  }
  const size_t stride = fiL1Stride(hdr.w);
  for (uint16_t y = 0; y < hdr.h; y++) {
    const uint8_t* row = pix + static_cast<size_t>(y) * stride;
    uint16_t* dst = gBlitRgb + static_cast<size_t>(y) * hdr.w;
    for (uint16_t x = 0; x < hdr.w; x++) {
      const bool on = (row[x / 8] & (0x80u >> (x & 7))) != 0;
      dst[x] = on ? fg : key;
    }
  }
  gfx.pushImage(x0, y0, hdr.w, hdr.h, gBlitRgb, key);
}

void blitR565(lgfx::LGFXBase& gfx, const FiHeader& hdr, const uint8_t* pix, int cx,
              int cy) {
  int x0 = 0;
  int y0 = 0;
  fiBlitOrigin(hdr, cx, cy, &x0, &y0);
  gfx.pushImage(x0, y0, hdr.w, hdr.h, reinterpret_cast<const uint16_t*>(pix));
}

bool blitP4(lgfx::LGFXBase& gfx, const FiHeader& hdr, const uint16_t* pal,
            const uint8_t* pix, int cx, int cy, uint16_t /*bg*/, uint16_t* outKey,
            bool* outHasKey) {
  int x0 = 0;
  int y0 = 0;
  fiBlitOrigin(hdr, cx, cy, &x0, &y0);
  const bool trans = (hdr.flags & kFiFlagHasTrans) != 0;
  const uint16_t tip = (pal && hdr.palette_len > 1) ? pal[1] : 0;
  const uint16_t key = fiPalTransKey(pal, hdr.palette_len, tip);
  if (outKey) {
    *outKey = key;
  }
  if (outHasKey) {
    *outHasKey = trans;
  }
  const size_t stride = (static_cast<size_t>(hdr.w) + 1u) / 2u;
  for (uint16_t y = 0; y < hdr.h; y++) {
    const uint8_t* row = pix + static_cast<size_t>(y) * stride;
    uint16_t* dst = gBlitRgb + static_cast<size_t>(y) * hdr.w;
    for (uint16_t x = 0; x < hdr.w; x++) {
      const uint8_t byte = row[x / 2];
      const uint8_t idx = (x & 1) ? (byte & 0x0f) : (byte >> 4);
      dst[x] = fiResolvePalPixel(idx, hdr.palette_len, pal, trans, key);
    }
  }
  if (trans) {
    gfx.pushImage(x0, y0, hdr.w, hdr.h, gBlitRgb, key);
  } else {
    gfx.pushImage(x0, y0, hdr.w, hdr.h, gBlitRgb);
  }
  return true;
}

bool blitP8(lgfx::LGFXBase& gfx, const FiHeader& hdr, const uint16_t* pal,
            const uint8_t* pix, int cx, int cy, uint16_t /*bg*/, uint16_t* outKey,
            bool* outHasKey) {
  int x0 = 0;
  int y0 = 0;
  fiBlitOrigin(hdr, cx, cy, &x0, &y0);
  const bool trans = (hdr.flags & kFiFlagHasTrans) != 0;
  const uint16_t tip = (pal && hdr.palette_len > 1) ? pal[1] : 0;
  const uint16_t key = fiPalTransKey(pal, hdr.palette_len, tip);
  if (outKey) {
    *outKey = key;
  }
  if (outHasKey) {
    *outHasKey = trans;
  }
  for (uint16_t y = 0; y < hdr.h; y++) {
    const uint8_t* row = pix + static_cast<size_t>(y) * hdr.w;
    uint16_t* dst = gBlitRgb + static_cast<size_t>(y) * hdr.w;
    for (uint16_t x = 0; x < hdr.w; x++) {
      dst[x] = fiResolvePalPixel(row[x], hdr.palette_len, pal, trans, key);
    }
  }
  if (trans) {
    gfx.pushImage(x0, y0, hdr.w, hdr.h, gBlitRgb, key);
  } else {
    gfx.pushImage(x0, y0, hdr.w, hdr.h, gBlitRgb);
  }
  return true;
}

}  // namespace

bool assetStoreBegin() {
  gReady = false;
  gEntryCount = 0;

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = kAssetMountPoint;
  conf.partition_label = "storage";
  conf.format_if_mount_failed = false;
  conf.grow_on_mount = true;

  const esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    LOGE("assets", "littlefs mount: %s", esp_err_to_name(err));
    return false;
  }

  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info("storage", &total, &used) == ESP_OK) {
    LOGI("assets", "littlefs total=%u used=%u", (unsigned)total, (unsigned)used);
  }

  if (!loadPack()) {
    return false;
  }
  gReady = true;
  return true;
}

bool assetStoreReady() {
  return gReady;
}

uint16_t assetStorePackVersion() {
  return gPackVer;
}

uint16_t assetStoreEntryCount() {
  return gEntryCount;
}

bool assetIconPresent(FlintIconId id, uint8_t sizePx) {
  if (!gReady || !gEntries) {
    return false;
  }
  return findEntry(static_cast<uint16_t>(id), sizePx) != nullptr;
}

bool assetBlitIcon(lgfx::LGFXBase& gfx, FlintIconId id, uint8_t sizePx, int cx,
                   int cy, uint16_t fg, uint16_t bg) {
  if (!gReady || !gEntries) {
    return false;
  }

  const uint16_t iconId = static_cast<uint16_t>(id);
  const FlintPackEntry* e = findEntry(iconId, sizePx);
  if (!e) {
    return false;
  }

  FiHeader hdr{};
  const uint16_t* pal = nullptr;
  const uint8_t* pix = nullptr;
  if (!loadFi(*e, &hdr, &pal, &pix)) {
    LOGE("assets", "fi load id=%u size=%u failed", (unsigned)id, (unsigned)sizePx);
    return false;
  }


  switch (static_cast<FiFormat>(hdr.format)) {
    case FiFormat::L1:
      blitL1(gfx, hdr, pix, cx, cy, fg, bg, nullptr);
      break;
    case FiFormat::R565:
      blitR565(gfx, hdr, pix, cx, cy);
      return true;
    case FiFormat::P4:
      if (!pal) {
        return false;
      }
      blitP4(gfx, hdr, pal, pix, cx, cy, bg, nullptr, nullptr);
      break;
    case FiFormat::P8:
      if (!pal) {
        return false;
      }
      blitP8(gfx, hdr, pal, pix, cx, cy, bg, nullptr, nullptr);
      break;
    default:
      return false;
  }

  return true;
}
