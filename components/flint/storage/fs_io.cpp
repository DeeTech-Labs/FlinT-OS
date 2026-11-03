#include "storage/fs_io.h"
#include "config/debug.h"
#include "esp_partition.h"
#include "esp_err.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <unistd.h>
bool fsPartitionErased(const char* label) {
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
  if (!partition) return false;
  uint8_t bytes[256];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(bytes)) {
    const size_t len = partition->size - offset < sizeof(bytes)
                           ? partition->size - offset : sizeof(bytes);
    if (esp_partition_read(partition, offset, bytes, len) != ESP_OK) return false;
    for (size_t i = 0; i < len; ++i) {
      if (bytes[i] != 0xff) return false;
    }
  }
  return true;
}

bool fsWriteAtomic(const char* path, const void* data, size_t len) {
  if (!path || (!data && len > 0)) {
    return false;
  }

  char tmpPath[96];
  const int n = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(tmpPath)) {
    LOGE("fs", "tmp path too long for %s", path);
    return false;
  }

  FILE* f = fopen(tmpPath, "wb");
  if (!f) {
    LOGE("fs", "fopen %s failed", tmpPath);
    return false;
  }

  if (len > 0 && fwrite(data, 1, len, f) != len) {
    LOGE("fs", "fwrite %s failed", tmpPath);
    fclose(f);
    unlink(tmpPath);
    return false;
  }

  bool committed = fflush(f) == 0;
  const int fd = fileno(f);
  if (fd < 0 || (committed && fsync(fd) != 0)) {
    committed = false;
  }
  if (fclose(f) != 0) committed = false;
  if (!committed) {
    LOGE("fs", "flush/close %s failed", tmpPath);
    unlink(tmpPath);
    return false;
  }

  if (rename(tmpPath, path) != 0) {
    LOGE("fs", "rename %s -> %s failed", tmpPath, path);
    unlink(tmpPath);
    return false;
  }
  return true;
}

int fsReadFile(const char* path, char* buf, size_t bufSize) {
  if (!path || !buf || bufSize < 2) {
    return -1;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    return -1;
  }

  const size_t maxRead = bufSize - 1;
  const size_t n = fread(buf, 1, maxRead, f);
  // Probe past the buffer limit so a valid prefix is never accepted as a
  // complete config. An exact fit (including the NUL) is still successful.
  const bool tooLong = n == maxRead && fgetc(f) != EOF;
  const bool readFailed = ferror(f) != 0;
  const bool closeFailed = fclose(f) != 0;
  if (tooLong || readFailed || closeFailed) {
    buf[0] = '\0';
    return -1;
  }
  buf[n] = '\0';
  return static_cast<int>(n);
}
