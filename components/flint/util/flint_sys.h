#pragma once

#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

inline uint32_t flintMillis() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

inline void flintDelay(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

inline uint32_t flintFreeHeap() {
  return esp_get_free_heap_size();
}
