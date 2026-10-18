#pragma once
#include <stdint.h>
struct FaceInfo { const char* id; const char* name; const char* author; uint16_t version; uint16_t minApi; };
namespace FaceRegistry { int count(); const FaceInfo* infoAt(int); bool setActiveId(const char*); }
