#pragma once
#include "esp_err.h"
esp_err_t esp_register_freertos_tick_hook_for_cpu(void (*)(), unsigned);
