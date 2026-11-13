#include "pwm_backlight.h"
namespace flint::modules {
esp_err_t PwmBacklight::set(unsigned percent) {
    return percent > 100 ? ESP_ERR_INVALID_ARG : output_.set((percent * 1023 + 50) / 100);
}
} // namespace flint::modules
