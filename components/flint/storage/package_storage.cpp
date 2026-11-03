#include "storage/package_storage.h"
#include "storage/fs_io.h"
#include "esp_littlefs.h"
#include "esp_err.h"
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
namespace {
bool ready=false;
bool allowed(const char* path){return path && !strncmp(path,"/storage/packages/",18) && !strstr(path,"..");}
}
bool packageStorageBegin(){
  ready=false;
  esp_vfs_littlefs_conf_t conf{};
  conf.base_path="/storage";conf.partition_label="storage";
  // Existing unreadable data is never treated as an empty filesystem.
  conf.format_if_mount_failed=fsPartitionErased("storage");conf.grow_on_mount=true;
  if(esp_vfs_littlefs_register(&conf)!=ESP_OK)return false;
  if(mkdir("/storage/packages",0775)!=0 && errno!=EEXIST)return false;
  ready=true;return true;
}
bool packageStorageInfo(PackageStorageInfo& info){
  size_t total=0,used=0;
  if(!ready||esp_littlefs_info("storage",&total,&used)!=ESP_OK||used>total)return false;
  info={total,used,total-used};return true;
}
bool packageStorageWriteAtomic(const char* path,const void* data,size_t size){return ready&&allowed(path)&&fsWriteAtomic(path,data,size);}
int packageStorageReadFile(const char* path,char* out,size_t capacity){return ready&&allowed(path)?fsReadFile(path,out,capacity):-1;}

bool packageStorageRemove(const char* path){return ready&&allowed(path)&&(unlink(path)==0||errno==ENOENT);}
