#include "driver/gpio.h"
#include "modules/battery_adc/battery_adc.h"
#include "modules/power_button/power_button.h"
#include "modules/pwm_backlight/pwm_backlight.h"
#include "modules/pwm_buzzer/pwm_buzzer.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
namespace {
std::array<unsigned, 2> duty{}, frequency{};
std::array<bool, 2> paused{};
std::array<int, 49> levels{};
bool failChannel{}, failCalibration{};
unsigned units{}, calibrations{}, samples{};
int rawValue = 2048;
} // namespace
esp_err_t ledc_timer_config(const ledc_timer_config_t *c) {
    CHECK(c->duty_resolution == 10);
    frequency[c->timer_num] = c->freq_hz;
    paused[c->timer_num] = false;
    return ESP_OK;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t *c) {
    CHECK(c->duty == 0);
    return failChannel ? ESP_FAIL : ESP_OK;
}
esp_err_t ledc_stop(int, int ch, int) {
    duty[ch] = 0;
    return ESP_OK;
}
esp_err_t ledc_timer_pause(int, int t) {
    paused[t] = true;
    return ESP_OK;
}
esp_err_t ledc_timer_resume(int, int t) {
    paused[t] = false;
    return ESP_OK;
}
uint32_t ledc_set_freq(int, int t, uint32_t f) {
    frequency[t] = f;
    return f;
}
esp_err_t ledc_set_duty(int, int ch, uint32_t value) {
    duty[ch] = value;
    return ESP_OK;
}
esp_err_t ledc_update_duty(int, int) {
    return ESP_OK;
}
esp_err_t adc_oneshot_io_to_channel(int gpio, int *unit, int *channel) {
    *unit = 1;
    *channel = gpio - 1;
    return ESP_OK;
}
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *, void **unit) {
    ++units;
    *unit = reinterpret_cast<void *>(1);
    return ESP_OK;
}
esp_err_t adc_oneshot_config_channel(void *, int, const adc_oneshot_chan_cfg_t *c) {
    CHECK(c->atten == 12);
    return ESP_OK;
}
esp_err_t adc_oneshot_del_unit(void *) {
    --units;
    return ESP_OK;
}
esp_err_t adc_oneshot_read(void *, int, int *raw) {
    ++samples;
    *raw = rawValue;
    return ESP_OK;
}
esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t *, void **cal) {
    if (failCalibration)
        return ESP_ERR_NOT_SUPPORTED;
    ++calibrations;
    *cal = reinterpret_cast<void *>(2);
    return ESP_OK;
}
esp_err_t adc_cali_delete_scheme_curve_fitting(void *) {
    --calibrations;
    return ESP_OK;
}
esp_err_t adc_cali_raw_to_voltage(void *, int, int *mv) {
    *mv = 1400;
    return ESP_OK;
}
esp_err_t gpio_set_level(int pin, int level) {
    levels[pin] = level;
    return ESP_OK;
}
int gpio_get_level(int pin) {
    return levels[pin];
}
esp_err_t gpio_config(const gpio_config_t *c) {
    if (c->mode == GPIO_MODE_OUTPUT)
        CHECK(levels[41] == 1);
    return ESP_OK;
}
int main() {
    using namespace flint::modules;
    {
        PwmBuzzer buzzer;
        CHECK(buzzer.begin(42) == ESP_OK && paused[0]);
        CHECK(buzzer.nextWake() == UINT64_MAX);
        CHECK(buzzer.tone(2000, 5001, 0) == ESP_ERR_INVALID_ARG);
        CHECK(buzzer.tone(2000, 100, 20) == ESP_OK && duty[0] == 512 && !paused[0]);
        CHECK(buzzer.nextWake() == 120);
        CHECK(buzzer.tone(3000, 5000, 21) == ESP_ERR_INVALID_STATE);
        CHECK(buzzer.maintain(119) == ESP_OK && duty[0] == 512);
        CHECK(buzzer.maintain(120) == ESP_OK && duty[0] == 0 && paused[0]);
        CHECK(buzzer.tone(2000, 100, 121) == ESP_ERR_INVALID_STATE);
        CHECK(buzzer.tone(2000, 5000, 220) == ESP_OK);
        CHECK(buzzer.tone(3000, 5000, 320) == ESP_OK && buzzer.nextWake() == 5220);
        CHECK(buzzer.maintain(5220) == ESP_OK);
        CHECK(buzzer.tone(2000, 100, 10219) == ESP_ERR_INVALID_STATE);
        CHECK(buzzer.tone(2000, 100, 10220) == ESP_OK);
        CHECK(buzzer.tone(2000, 0, 10221) == ESP_OK);
        CHECK(buzzer.nextWake() == UINT64_MAX);
        CHECK(buzzer.tone(500, 0, 0) == ESP_OK && duty[0] == 0);
        PwmBacklight light;
        CHECK(light.begin(15) == ESP_OK && paused[1]);
        CHECK(light.set(100) == ESP_OK && duty[1] == 1023 && !paused[1]);
        CHECK(light.set(101) == ESP_ERR_INVALID_ARG);
        CHECK(light.set(0) == ESP_OK && duty[1] == 0 && paused[1]);
    }
    failChannel = true;
    {
        PwmBuzzer bad;
        CHECK(bad.begin(42) == ESP_FAIL && !bad.ready() && paused[0]);
    }
    failChannel = false;
    {
        BatteryAdc battery;
        CHECK(battery.begin(1, 3, 1) == ESP_OK);
        int mv{};
        CHECK(battery.millivolts(mv) == ESP_OK && mv == 4200 && samples == 8);
        rawValue = 4095;
        CHECK(battery.millivolts(mv) == ESP_ERR_INVALID_RESPONSE && mv == 0);
    }
    CHECK(!units && !calibrations);
    failCalibration = true;
    {
        BatteryAdc bad;
        CHECK(bad.begin(1, 3, 1) == ESP_ERR_NOT_SUPPORTED && !bad.ready());
    }
    CHECK(!units && !calibrations);
    PowerButton power;
    CHECK(power.begin(41, 40, 0) == ESP_OK && levels[41] == 1);
    levels[40] = 0;
    levels[0] = 1;
    CHECK(power.pressed() && !power.bootPressed());
    CHECK(power.releasePower() == ESP_OK && levels[41] == 0);
    std::puts("PWM deadlines/gating, calibrated battery ADC and power hold: passed");
}
