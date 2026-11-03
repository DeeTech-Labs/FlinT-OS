#pragma once
#include <stddef.h>
bool fsPartitionErased(const char* label);
bool fsWriteAtomic(const char* path,const void* data,size_t size);
int fsReadFile(const char* path,char* out,size_t capacity);
