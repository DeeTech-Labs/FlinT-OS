#pragma once
#include "esp_err.h"
#include <cstdint>
using gpio_num_t = int;
inline constexpr int GPIO_MODE_OUTPUT = 1;
struct gpio_config_t {
    uint64_t pin_bit_mask{};
    int mode{};
};
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_set_level(gpio_num_t, uint32_t);
