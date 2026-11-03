#pragma once
#include <stddef.h>
struct PackageStorageInfo { size_t total=0,used=0,free=0; };
bool packageStorageBegin();
bool packageStorageInfo(PackageStorageInfo& info);
bool packageStorageWriteAtomic(const char* path,const void* data,size_t size);
int packageStorageReadFile(const char* path,char* out,size_t capacity);

bool packageStorageRemove(const char* path);
