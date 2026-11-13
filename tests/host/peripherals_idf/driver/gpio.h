#pragma once
#include "esp_err.h"
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) <= 48)
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) GPIO_IS_VALID_GPIO(pin)
inline constexpr int GPIO_MODE_OUTPUT = 1, GPIO_MODE_INPUT = 2, GPIO_PULLUP_ENABLE = 1;
struct gpio_config_t {
    uint64_t pin_bit_mask{};
    int mode{}, pull_up_en{};
};
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_set_level(int, int);
int gpio_get_level(int);
