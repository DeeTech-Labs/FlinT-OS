#pragma once
#include <cstddef>
#include <cstdint>
inline constexpr uint32_t MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_INTERNAL = 2, MALLOC_CAP_8BIT = 4;
void *heap_caps_malloc(size_t, uint32_t);
void heap_caps_free(void *);
