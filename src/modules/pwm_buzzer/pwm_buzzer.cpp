#include "pwm_buzzer.h"
#include <algorithm>
namespace flint::modules {
esp_err_t PwmBuzzer::tone(unsigned frequency, unsigned durationMs, uint64_t now) {
    if (durationMs > 5000 || frequency < 100 || frequency > 10000 || now > UINT64_MAX - 10000)
        return ESP_ERR_INVALID_ARG;
    if (!durationMs) {
        const auto error = output_.set(0);
        if (error == ESP_OK && stopAt_ != UINT64_MAX) {
            quietUntil_ = now + std::max<uint64_t>(100, now - startedAt_);
            stopAt_ = UINT64_MAX;
        }
        return error;
    }
    auto error = maintain(now);
    if (error != ESP_OK)
        return error;
    if (now < quietUntil_ || (commanded_ && now - lastCommand_ < 100))
        return ESP_ERR_INVALID_STATE;
    const bool starting = stopAt_ == UINT64_MAX;
    error = output_.set(512, frequency);
    if (error == ESP_OK) {
        if (starting)
            startedAt_ = now;
        // Retuning cannot extend one continuous sound beyond five seconds.
        stopAt_ = std::min(now + durationMs, startedAt_ + 5000);
        lastCommand_ = now;
        commanded_ = true;
    }
    return error;
}
esp_err_t PwmBuzzer::maintain(uint64_t now) {
    if (now < stopAt_)
        return ESP_OK;
    const auto error = output_.set(0);
    if (error == ESP_OK) {
        // At most 50% long-term duty, at least 100ms quiet between bursts.
        quietUntil_ = now + std::max<uint64_t>(100, now - startedAt_);
        stopAt_ = UINT64_MAX;
    }
    return error;
}
} // namespace flint::modules
