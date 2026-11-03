#include "settings/cfg_fs.h"
#include "storage/fs_io.h"
#include "config/debug.h"

#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_partition.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

bool gReady = false;

// Only a fully erased partition may be initialized automatically. Read errors
// and existing data must never be interpreted as permission to format.

}  // namespace

bool cfgFsBegin() {
  gReady = false;

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = kCfgMountPoint;
  conf.partition_label = kCfgPartitionLabel;
  conf.format_if_mount_failed = fsPartitionErased(kCfgPartitionLabel);
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

bool cfgFsWriteAtomic(const char* path,const void* data,size_t size) {
  return gReady && fsWriteAtomic(path,data,size);
}
int cfgFsReadFile(const char* path,char* out,size_t capacity) {
  return gReady ? fsReadFile(path,out,capacity) : -1;
}
