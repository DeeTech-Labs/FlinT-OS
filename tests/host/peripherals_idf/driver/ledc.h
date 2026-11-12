#pragma once
#include "esp_err.h"
#include <cstdint>
using ledc_timer_t = int;
using ledc_channel_t = int;
inline constexpr int LEDC_TIMER_0 = 0, LEDC_TIMER_1 = 1, LEDC_CHANNEL_0 = 0, LEDC_CHANNEL_1 = 1;
inline constexpr int LEDC_LOW_SPEED_MODE = 0, LEDC_TIMER_10_BIT = 10, LEDC_AUTO_CLK = 0;
struct ledc_timer_config_t {
    int speed_mode{}, duty_resolution{}, timer_num{};
    unsigned freq_hz{};
    int clk_cfg{};
};
struct ledc_channel_config_t {
    int gpio_num{}, speed_mode{}, channel{}, timer_sel{};
    unsigned duty{};
};
esp_err_t ledc_timer_config(const ledc_timer_config_t *);
esp_err_t ledc_channel_config(const ledc_channel_config_t *);
esp_err_t ledc_stop(int, int, int);
esp_err_t ledc_timer_pause(int, int);
esp_err_t ledc_timer_resume(int, int);
uint32_t ledc_set_freq(int, int, uint32_t);
esp_err_t ledc_set_duty(int, int, uint32_t);
esp_err_t ledc_update_duty(int, int);
