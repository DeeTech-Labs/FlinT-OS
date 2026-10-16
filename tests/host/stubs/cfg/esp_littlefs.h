#pragma once
#include <stddef.h>
#include "esp_err.h"
struct esp_vfs_littlefs_conf_t {
  const char* base_path;
  const char* partition_label;
  bool format_if_mount_failed;
  bool grow_on_mount;
};
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t*);
esp_err_t esp_littlefs_info(const char*, size_t*, size_t*);
