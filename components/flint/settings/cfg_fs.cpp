#include "settings/cfg_fs.h"
#include "config/debug.h"

#include "esp_err.h"
#include "esp_littlefs.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

bool gReady = false;

}  // namespace

bool cfgFsBegin() {
  gReady = false;

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = kCfgMountPoint;
  conf.partition_label = kCfgPartitionLabel;
  conf.format_if_mount_failed = true;
  conf.grow_on_mount = true;

  const esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    LOGE("cfg", "littlefs mount: %s", esp_err_to_name(err));
    return false;
  }

  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(kCfgPartitionLabel, &total, &used) == ESP_OK) {
    LOGI("cfg", "littlefs total=%u used=%u", (unsigned)total, (unsigned)used);
  }

  gReady = true;
  return true;
}

bool cfgFsReady() {
  return gReady;
}

bool cfgFsWriteAtomic(const char* path, const void* data, size_t len) {
  if (!gReady || !path || (!data && len > 0)) {
    return false;
  }

  char tmpPath[96];
  const int n = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(tmpPath)) {
    LOGE("cfg", "tmp path too long for %s", path);
    return false;
  }

  FILE* f = fopen(tmpPath, "wb");
  if (!f) {
    LOGE("cfg", "fopen %s failed", tmpPath);
    return false;
  }

  if (len > 0 && fwrite(data, 1, len, f) != len) {
    LOGE("cfg", "fwrite %s failed", tmpPath);
    fclose(f);
    unlink(tmpPath);
    return false;
  }

  fflush(f);
  const int fd = fileno(f);
  if (fd >= 0) {
    fsync(fd);
  }
  fclose(f);

  if (rename(tmpPath, path) != 0) {
    LOGE("cfg", "rename %s -> %s failed", tmpPath, path);
    unlink(tmpPath);
    return false;
  }
  return true;
}

int cfgFsReadFile(const char* path, char* buf, size_t bufSize) {
  if (!gReady || !path || !buf || bufSize < 2) {
    return -1;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    return -1;
  }

  const size_t maxRead = bufSize - 1;
  const size_t n = fread(buf, 1, maxRead, f);
  fclose(f);
  buf[n] = '\0';
  return static_cast<int>(n);
}
