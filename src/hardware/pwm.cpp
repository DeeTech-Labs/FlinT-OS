#include "pwm.h"
#include "driver/gpio.h"
#include "esp_log.h"
namespace flint::hardware {
PwmOutput::~PwmOutput() {
    if (ready_ && ledc_stop(LEDC_LOW_SPEED_MODE, channel_, 0) != ESP_OK)
        ESP_LOGE("pwm", "stop failed");
    if (ready_ && ledc_timer_pause(LEDC_LOW_SPEED_MODE, timer_) != ESP_OK)
        ESP_LOGE("pwm", "timer pause failed");
}
esp_err_t PwmOutput::begin(
    int gpio, ledc_timer_t timer, ledc_channel_t channel, unsigned frequency) {
    if (ready_)
        return ESP_ERR_INVALID_STATE;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio) || frequency < 100 || frequency > 20000)
        return ESP_ERR_INVALID_ARG;
    ledc_timer_config_t config{};
    config.speed_mode = LEDC_LOW_SPEED_MODE;
    config.duty_resolution = LEDC_TIMER_10_BIT;
    config.timer_num = timer;
    config.freq_hz = frequency;
    config.clk_cfg = LEDC_AUTO_CLK;
    auto error = ledc_timer_config(&config);
    if (error != ESP_OK)
        return error;
    ledc_channel_config_t output{};
    output.gpio_num = gpio;
    output.speed_mode = config.speed_mode;
    output.channel = channel;
    output.timer_sel = timer;
    output.duty = 0;
    error = ledc_channel_config(&output);
    if (error != ESP_OK) {
        if (ledc_timer_pause(config.speed_mode, timer) != ESP_OK)
            ESP_LOGE("pwm", "partial initialization cleanup failed");
        return error;
    }
    timer_ = timer;
    channel_ = channel;
    ready_ = true;
    const auto pause = ledc_timer_pause(LEDC_LOW_SPEED_MODE, timer_);
    if (pause != ESP_OK) {
        if (ledc_stop(LEDC_LOW_SPEED_MODE, channel_, 0) != ESP_OK)
            ESP_LOGE("pwm", "initialization cleanup failed");
        ready_ = false;
    }
    return pause;
}
esp_err_t PwmOutput::set(unsigned duty, unsigned frequency) {
    if (!ready_)
        return ESP_ERR_INVALID_STATE;
    if (duty > 1023 || (frequency && (frequency < 100 || frequency > 20000)))
        return ESP_ERR_INVALID_ARG;
    if (duty) {
        const auto error = ledc_timer_resume(LEDC_LOW_SPEED_MODE, timer_);
        if (error != ESP_OK)
            return error;
    }
    if (frequency && !ledc_set_freq(LEDC_LOW_SPEED_MODE, timer_, frequency))
        return ESP_FAIL;
    auto error = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
    if (error == ESP_OK)
        error = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
    if (error == ESP_OK && !duty)
        error = ledc_timer_pause(LEDC_LOW_SPEED_MODE, timer_);
    return error;
}
} // namespace flint::hardware
