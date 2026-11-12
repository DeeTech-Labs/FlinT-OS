#pragma once
#include "driver/ledc.h"
namespace flint::hardware {
class PwmOutput {
public:
    PwmOutput() = default;
    ~PwmOutput();
    PwmOutput(const PwmOutput &) = delete;
    PwmOutput &operator=(const PwmOutput &) = delete;
    esp_err_t begin(int gpio, ledc_timer_t timer, ledc_channel_t channel, unsigned frequency);
    esp_err_t set(unsigned duty, unsigned frequency = 0);
    bool ready() const {
        return ready_;
    }

private:
    ledc_timer_t timer_{};
    ledc_channel_t channel_{};
    bool ready_{};
};
} // namespace flint::hardware
