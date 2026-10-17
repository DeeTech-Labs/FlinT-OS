#include "settings/cfg_fs.h"
#include "host_check.h"
#include "esp_partition.h"
#include "esp_littlefs.h"
#include <cstring>
#include <string>
#include <unistd.h>

static unsigned char flash[513];
static bool readFail = false;
static bool partitionMissing = false;
static int mountResult = ESP_OK;
static bool allowFormat = false;
static int failStage = 0;
const esp_partition_t* esp_partition_find_first(int, int, const char*) {
  static esp_partition_t p{sizeof(flash)};
  return partitionMissing ? nullptr : &p;
}
esp_err_t esp_partition_read(const esp_partition_t*, size_t offset, void* buf, size_t n) {
  if (readFail) return -1;
  std::memcpy(buf, flash + offset, n);
  return ESP_OK;
}
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* c) {
  allowFormat = c->format_if_mount_failed;
  return mountResult;
}
esp_err_t esp_littlefs_info(const char*, size_t* total, size_t* used) {
  *total = sizeof(flash);
  *used = 0;
  return ESP_OK;
}
extern "C" size_t __real_fread(void*, size_t, size_t, FILE*);
extern "C" int __real_ferror(FILE*);
extern "C" size_t __wrap_fread(void* buf, size_t size, size_t count, FILE* f) {
  return __real_fread(buf, size, failStage == 4 && count > 1 ? 1 : count, f);
}
extern "C" int __wrap_ferror(FILE* f) {
  return failStage == 4 ? 1 : __real_ferror(f);
}
extern "C" int __real_fflush(FILE*);
extern "C" int __real_fsync(int);
extern "C" int __real_fclose(FILE*);
extern "C" int __wrap_fflush(FILE* f) {
  return failStage == 1 ? EOF : __real_fflush(f);
}
extern "C" int __wrap_fsync(int fd) {
  return failStage == 2 ? -1 : __real_fsync(fd);
}
extern "C" int __wrap_fclose(FILE* f) {
  const int result = __real_fclose(f);
  return failStage == 3 ? EOF : result;
}
int main() {
  std::memset(flash, 0xff, sizeof(flash));
  hostRequire(cfgFsBegin() && allowFormat, "initialize erased partition");
  flash[512] = 0;
  hostRequire(cfgFsBegin() && !allowFormat, "preserve data even in last partial block");
  std::memset(flash, 0xff, sizeof(flash));
  readFail = true;
  hostRequire(cfgFsBegin() && !allowFormat, "read error must not permit format");

  readFail = false;
  partitionMissing = true;
  mountResult = -1;
  hostRequire(!cfgFsBegin() && !cfgFsReady() && !allowFormat,
              "missing partition must not format or become ready");
  partitionMissing = false;
  flash[0] = 0;
  hostRequire(!cfgFsBegin() && !cfgFsReady() && !allowFormat,
              "mount failure must preserve existing data");
  mountResult = ESP_OK;
  hostRequire(cfgFsBegin(), "restore mount for write tests");

  char dir[] = "/tmp/flint-cfg-test-XXXXXX";
  hostRequire(mkdtemp(dir) != nullptr, "temporary directory");
  const std::string path = std::string(dir) + "/ui.cfg";
  hostRequire(cfgFsWriteAtomic(path.c_str(), "old", 3), "initial write");
  for (int stage = 1; stage <= 3; ++stage) {
    failStage = stage;
    hostRequire(!cfgFsWriteAtomic(path.c_str(), "new", 3), "report flush/sync/close failure");
    failStage = 0;
    char buf[16];
    hostRequire(cfgFsReadFile(path.c_str(), buf, sizeof(buf)) == 3 &&
                std::strcmp(buf, "old") == 0, "preserve original on failure");
    hostRequire(access((path + ".tmp").c_str(), F_OK) != 0, "remove failed temporary file");
  }
  hostRequire(cfgFsWriteAtomic(path.c_str(), "new", 3), "successful replacement");
  char buf[16];
  hostRequire(cfgFsReadFile(path.c_str(), buf, sizeof(buf)) == 3 &&
              std::strcmp(buf, "new") == 0, "read replacement");
  char exact[4];
  hostRequire(cfgFsReadFile(path.c_str(), exact, sizeof(exact)) == 3 &&
              std::strcmp(exact, "new") == 0, "accept exact fit with terminator");
  char small[3] = "xx";
  hostRequire(cfgFsReadFile(path.c_str(), small, sizeof(small)) == -1 && small[0] == '\0',
              "reject truncated config and clear partial contents");
  for (int stage : {3, 4}) {
    failStage = stage;
    hostRequire(cfgFsReadFile(path.c_str(), buf, sizeof(buf)) == -1 && buf[0] == '\0',
                "reject read/close failure and clear partial contents");
  }
  failStage = 0;
  hostRequire(cfgFsWriteAtomic(path.c_str(), "", 0), "write empty config");
  hostRequire(cfgFsReadFile(path.c_str(), buf, sizeof(buf)) == 0 && buf[0] == '\0',
              "accept empty file");
  unlink(path.c_str());
  rmdir(dir);
  std::puts("ok test_cfg_fs");
}
