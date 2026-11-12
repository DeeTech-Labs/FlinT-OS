#include "pwm_buzzer.h"
namespace flint::modules {
esp_err_t PwmBuzzer::tone(unsigned frequency, unsigned durationMs, uint64_t now) {
    if (durationMs > 5000 || frequency < 100 || frequency > 10000 || now > UINT64_MAX - durationMs)
        return ESP_ERR_INVALID_ARG;
    const auto error = output_.set(durationMs ? 512 : 0, frequency);
    if (error == ESP_OK)
        stopAt_ = durationMs ? now + durationMs : UINT64_MAX;
    return error;
}
esp_err_t PwmBuzzer::maintain(uint64_t now) {
    if (now < stopAt_)
        return ESP_OK;
    stopAt_ = UINT64_MAX;
    return output_.set(0);
}
} // namespace flint::modules
