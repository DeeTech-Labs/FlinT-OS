#include "power_button.h"
#include "driver/gpio.h"
namespace flint::modules {
esp_err_t PowerButton::begin(int hold, int button, int boot) {
    if (ready())
        return ESP_ERR_INVALID_STATE;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(hold) || !GPIO_IS_VALID_GPIO(button) ||
        !GPIO_IS_VALID_GPIO(boot) || hold == button || hold == boot || button == boot)
        return ESP_ERR_INVALID_ARG;
    // Preload HIGH before switching the hold pin to output, avoiding a power-off glitch.
    auto error = gpio_set_level(gpio_num_t(hold), 1);
    gpio_config_t output{};
    output.pin_bit_mask = 1ULL << hold;
    output.mode = GPIO_MODE_OUTPUT;
    if (error == ESP_OK)
        error = gpio_config(&output);
    gpio_config_t input{};
    input.pin_bit_mask = (1ULL << button) | (1ULL << boot);
    input.mode = GPIO_MODE_INPUT;
    input.pull_up_en = GPIO_PULLUP_ENABLE;
    if (error == ESP_OK)
        error = gpio_config(&input);
    if (error == ESP_OK) {
        hold_ = hold;
        button_ = button;
        boot_ = boot;
    }
    return error;
}
esp_err_t PowerButton::releasePower() {
    return ready() ? gpio_set_level(gpio_num_t(hold_), 0) : ESP_ERR_INVALID_STATE;
}
bool PowerButton::pressed() const {
    return ready() && !gpio_get_level(gpio_num_t(button_));
}
bool PowerButton::bootPressed() const {
    return ready() && !gpio_get_level(gpio_num_t(boot_));
}
} // namespace flint::modules
