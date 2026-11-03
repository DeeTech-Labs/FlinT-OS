#include "companion/ignits.h"
#include "settings/cfg_fs.h"
#include "storage/package_storage.h"
#include "render/builders.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
static std::map<std::string,std::string> files;
static bool storageOkay=true;
static size_t freeBytes=60000;
static uint32_t freeRam=60000;
uint32_t ignitFreeRam(){return freeRam;}
bool packageStorageInfo(PackageStorageInfo& info){info={65536,65536-freeBytes,freeBytes};return true;}
bool cfgFsWriteAtomic(const char* p,const void* data,size_t length) {
  if(!storageOkay)return false;
  files[p]=std::string(static_cast<const char*>(data),length);return true;
}
int cfgFsReadFile(const char* p,char* out,size_t n) {
  if(!files.count(p)||files[p].size()>=n)return -1;
  strcpy(out,files[p].c_str());return files[p].size();
}
bool packageStorageRemove(const char* path){if(!storageOkay)return false;files.erase(path);return true;}
bool packageStorageWriteAtomic(const char* path,const void* data,size_t n){return cfgFsWriteAtomic(path,data,n);}
int packageStorageReadFile(const char* path,char* out,size_t capacity){return cfgFsReadFile(path,out,capacity);}
static std::string package(const char* id="pkg.mint",int version=1,int kind=2) {
  return std::string("1 ")+id+" 4d696e74 "+std::to_string(version)+" "+std::to_string(kind)+" 1 080810 101820 77ddbb 99aaaa f4f4ee 183038";
}
int main() {
  IgnitDefinition p;
  assert(ignitParse(package().c_str(),p)&&!strcmp(p.id,"pkg.mint")&&!strcmp(p.name,"Mint"));
  for(const char* bad:{"", "2 pkg.x 41 1 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x 00 1 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x c080 1 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x eda080 1 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 ../x 41 1 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x 41 65536 0 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x 41 1 9 0 080810 101820 77ddbb 99aaaa f4f4ee 183038",
      "1 pkg.x 41 1 0 3 080810 101820 77ddbb 99aaaa f4f4ee 183038"}) assert(!ignitParse(bad,p));
  ignitsBegin();assert(ignitsCount()==0);
  assert(!strcmp(ignitRemove("digital"),"protected_package"));
  freeRam=1024;
  assert(!strcmp(ignitInstall(package().c_str()),"insufficient_ram"));
  freeRam=60000;
  freeBytes=1024;
  assert(!strcmp(ignitInstall(package().c_str()),"insufficient_storage"));
  assert(ignitsCount()==0);
  freeBytes=60000;
  assert(!ignitInstall(package().c_str()));
  assert(!ignitInstall(package("pkg.two").c_str()));
  assert(!ignitInstall(package("pkg.three").c_str()));
  assert(!strcmp(ignitInstall(package("pkg.four").c_str()),"package_storage_full"));
  storageOkay=false;
  assert(!strcmp(ignitInstall(package("pkg.mint",2).c_str()),"storage_error"));
  assert(ignitFind("pkg.mint")->version==1);
  storageOkay=true;
  assert(!ignitInstall(package("pkg.mint",2).c_str()));
  assert(!strcmp(ignitInstall(package().c_str()),"package_downgrade"));
  assert(!strcmp(ignitInstall(package("pkg.mint",2,0).c_str()),"package_kind_changed"));
  assert(!ignitThemeUse("pkg.mint"));
  assert(FlintRender::Accent==FlintRender::rgb24(0x77ddbb));
  ignitsBegin();ignitThemeRestore(); // package and theme survive a restart
  assert(ignitsCount()==3&&ignitFind("pkg.mint")->version==2);
  assert(FlintRender::Accent==FlintRender::rgb24(0x77ddbb));
  assert(!ignitRemove("pkg.mint"));ignitThemeRestore();
  assert(!ignitFind("pkg.mint")&&FlintRender::Accent==0x7ef5);
  assert(!ignitInstall(package("pkg.new").c_str()));
  ignitsBegin();assert(ignitsCount()==3);
  assert(!ignitThemeUse(""));
  files["/storage/packages/ignit0.ignit"]="corrupt";ignitsBegin();assert(ignitsCount()==2);
}
