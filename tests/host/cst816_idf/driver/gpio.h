#pragma once
#include "i2c_master.h"
#define GPIO_IS_VALID_OUTPUT_GPIO(n) ((n) >= 0 && (n) < 49)
#define GPIO_IS_VALID_GPIO(n) GPIO_IS_VALID_OUTPUT_GPIO(n)
inline constexpr int GPIO_MODE_OUTPUT = 1, GPIO_MODE_INPUT = 2, GPIO_INTR_DISABLE = 0,
                     GPIO_INTR_NEGEDGE = 1;
struct gpio_config_t {
    uint64_t pin_bit_mask{};
    int mode{}, intr_type{};
};
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_set_level(gpio_num_t, int);
esp_err_t gpio_intr_disable(gpio_num_t);
esp_err_t gpio_intr_enable(gpio_num_t);
esp_err_t gpio_set_intr_type(gpio_num_t, int);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_isr_handler_add(gpio_num_t, void (*)(void *), void *);
esp_err_t gpio_isr_handler_remove(gpio_num_t);
