#include "peripherals.h"
#include "board/device_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#if FLINT_HAS_I2C
#include "i2c.h"
#endif
#if FLINT_HAS_RTC
#include "modules/pcf85063/pcf85063.h"
#endif
#if FLINT_HAS_IMU
#include "modules/qmi8658/qmi8658.h"
#endif
#if FLINT_HAS_BATTERY
#include "modules/battery_adc/battery_adc.h"
#endif
#if FLINT_HAS_BUZZER
#include "modules/pwm_buzzer/pwm_buzzer.h"
#endif
#if FLINT_HAS_BACKLIGHT
#include "modules/pwm_backlight/pwm_backlight.h"
#endif
#if FLINT_HAS_POWER_BUTTON
#include "modules/power_button/power_button.h"
#endif
#if FLINT_HAS_BMP280
#include "modules/bmp280/bmp280.h"
#endif
#if FLINT_HAS_BME280
#include "modules/bme280/bme280.h"
#endif
#if FLINT_HAS_LIGHT
#include "modules/veml7700/veml7700.h"
#endif
#include <algorithm>
namespace {
#if FLINT_HAS_RTC
flint::modules::Pcf85063 rtc;
#endif
#if FLINT_HAS_IMU
flint::modules::Qmi8658 imu;
#endif
#if FLINT_HAS_BATTERY
flint::modules::BatteryAdc battery;
#endif
#if FLINT_HAS_BUZZER
flint::modules::PwmBuzzer buzzer;
#endif
#if FLINT_HAS_BACKLIGHT
flint::modules::PwmBacklight backlight;
#endif
#if FLINT_HAS_POWER_BUTTON
flint::modules::PowerButton power;
#endif
#if FLINT_HAS_BMP280
flint::modules::Bmp280 bmp;
flint::modules::EnvironmentalSample bmpSample;
esp_err_t bmpStatus = ESP_ERR_INVALID_STATE;
#endif
#if FLINT_HAS_BME280
flint::modules::Bme280 bme;
flint::modules::EnvironmentalSample bmeSample;
esp_err_t bmeStatus = ESP_ERR_INVALID_STATE;
#endif
#if FLINT_HAS_LIGHT
flint::modules::Veml7700 light;
double lightLux{};
esp_err_t lightStatus = ESP_ERR_INVALID_STATE;
#endif
[[maybe_unused]] void report(const char *module, esp_err_t error) {
    if (error != ESP_OK)
        ESP_LOGE("hardware", "%s: %s", module, esp_err_to_name(error));
}
} // namespace
void deviceHardwareBegin() {
#if FLINT_HAS_POWER_BUTTON
    report("power",
        power.begin(FLINT_POWER_BUTTON_HOLD, FLINT_POWER_BUTTON_BUTTON, FLINT_POWER_BUTTON_BOOT));
#endif
#if FLINT_HAS_BMP280
    report("bmp280", bmp.begin(deviceI2cBus(), FLINT_BMP280_ADDRESS));
#endif
#if FLINT_HAS_BME280
    report("bme280", bme.begin(deviceI2cBus(), FLINT_BME280_ADDRESS));
#endif
#if FLINT_HAS_LIGHT
    report("veml7700", light.begin(deviceI2cBus()));
#endif
#if FLINT_HAS_RTC
    report("rtc", rtc.begin(deviceI2cBus(), FLINT_RTC_ADDRESS));
#endif
#if FLINT_HAS_IMU
    report("imu", imu.begin(deviceI2cBus(), FLINT_IMU_ADDRESS));
#endif
#if FLINT_HAS_BATTERY
    report("battery",
        battery.begin(
            FLINT_BATTERY_ADC, FLINT_BATTERY_DIVIDER_NUMERATOR, FLINT_BATTERY_DIVIDER_DENOMINATOR));
#endif
#if FLINT_HAS_BUZZER
    report("buzzer", buzzer.begin(FLINT_BUZZER_OUTPUT));
#endif
#if FLINT_HAS_BACKLIGHT
    report("backlight", backlight.begin(FLINT_BACKLIGHT_OUTPUT));
#endif
}
void deviceHardwareDiagnostics(flint::Values &values) {
#if FLINT_HAS_RTC
    std::tm utc{};
    values["rtc_ready"] = rtc.ready() ? "1" : "0";
    const auto error = rtc.read(utc);
    values["rtc_time_valid"] = error == ESP_OK ? "1" : "0";
    if (error == ESP_OK) {
        char text[32]{};
        if (std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc))
            values["rtc_utc"] = text;
    }
#endif
#if FLINT_HAS_IMU
    values["imu_ready"] = imu.ready() ? "1" : "0";
#endif
#if FLINT_HAS_BATTERY
    int mv{};
    values["battery_ready"] = battery.ready() ? "1" : "0";
    if (battery.millivolts(mv) == ESP_OK)
        values["battery_voltage_mv"] = std::to_string(mv);
#endif
#if FLINT_HAS_BUZZER
    values["buzzer_ready"] = buzzer.ready() ? "1" : "0";
#endif
#if FLINT_HAS_BACKLIGHT
    values["backlight_ready"] = backlight.ready() ? "1" : "0";
#endif
#if FLINT_HAS_POWER_BUTTON
    values["power_control_ready"] = power.ready() ? "1" : "0";
    if (power.ready()) {
        values["power_button_raw"] = power.pressed() ? "1" : "0";
        values["boot_button_raw"] = power.bootPressed() ? "1" : "0";
    }
#endif
#if FLINT_HAS_BMP280
    values["bmp280_ready"] = bmp.ready() ? "1" : "0";
    if (bmpStatus == ESP_OK) {
        values["bmp280_temperature_c"] = std::to_string(bmpSample.temperatureC);
        values["bmp280_pressure_pa"] = std::to_string(bmpSample.pressurePa);
    }
#endif
#if FLINT_HAS_BME280
    values["bme280_ready"] = bme.ready() ? "1" : "0";
    if (bmeStatus == ESP_OK) {
        values["bme280_temperature_c"] = std::to_string(bmeSample.temperatureC);
        values["bme280_pressure_pa"] = std::to_string(bmeSample.pressurePa);
        values["bme280_humidity_percent"] = std::to_string(bmeSample.humidityPercent);
    }
#endif
#if FLINT_HAS_LIGHT
    values["veml7700_ready"] = light.ready() ? "1" : "0";
    if (lightStatus == ESP_OK)
        values["ambient_lux"] = std::to_string(lightLux);
#endif
    (void)values;
}
flint::Error devicePowerStatus(std::string_view key, flint::AppReply &reply) {
#if FLINT_HAS_BATTERY
    if (key != "voltage_mv")
        return flint::Error::not_supported;
    int mv{};
    if (battery.millivolts(mv) != ESP_OK)
        return flint::Error::source_unavailable;
    reply.number = mv;
    reply.numeric = true;
    return flint::Error::ok;
#else
    (void)key;
    (void)reply;
    return flint::Error::not_supported;
#endif
}
esp_err_t deviceBacklight(unsigned percent) {
#if FLINT_HAS_BACKLIGHT
    return backlight.set(percent);
#else
    (void)percent;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceTone(unsigned frequency, unsigned durationMs) {
#if FLINT_HAS_BUZZER
    return buzzer.tone(frequency, durationMs, uint64_t(esp_timer_get_time() / 1000));
#else
    (void)frequency;
    (void)durationMs;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
void deviceHardwareMaintain() {
    [[maybe_unused]] const auto at = uint64_t(esp_timer_get_time() / 1000);
#if FLINT_HAS_BUZZER
    report("buzzer stop", buzzer.maintain(at));
#endif
#if FLINT_HAS_BMP280
    if (at >= bmp.nextWake())
        bmpStatus = bmp.read(bmpSample, at);
#endif
#if FLINT_HAS_BME280
    if (at >= bme.nextWake())
        bmeStatus = bme.read(bmeSample, at);
#endif
#if FLINT_HAS_LIGHT
    if (at >= light.nextWake())
        lightStatus = light.read(lightLux, at);
#endif
}
uint64_t deviceHardwareNextWake() {
    uint64_t next = UINT64_MAX;
#if FLINT_HAS_BUZZER
    next = std::min(next, buzzer.nextWake());
#endif
#if FLINT_HAS_BMP280
    next = std::min(next, bmp.nextWake());
#endif
#if FLINT_HAS_BME280
    next = std::min(next, bme.nextWake());
#endif
#if FLINT_HAS_LIGHT
    next = std::min(next, light.nextWake());
#endif
    return next;
}
esp_err_t deviceMotionEnable(bool enabled) {
#if FLINT_HAS_IMU
    return imu.enable(enabled);
#else
    (void)enabled;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceMotionRead(flint::modules::MotionSample &sample) {
#if FLINT_HAS_IMU
    return imu.read(sample);
#else
    (void)sample;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceRtcRead(std::tm &utc) {
#if FLINT_HAS_RTC
    return rtc.read(utc);
#else
    utc = {};
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceRtcSet(const std::tm &utc) {
#if FLINT_HAS_RTC
    return rtc.set(utc);
#else
    (void)utc;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceReleasePower() {
#if FLINT_HAS_POWER_BUTTON
    return power.releasePower();
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t deviceEnvironmentStart(EnvironmentSensor sensor) {
    [[maybe_unused]] const auto at = uint64_t(esp_timer_get_time() / 1000);
    switch (sensor) {
    case EnvironmentSensor::bmp280:
#if FLINT_HAS_BMP280
    {
        const auto error = bmp.start(at);
        if (error == ESP_OK)
            bmpStatus = ESP_ERR_NOT_FOUND;
        return error;
    }
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case EnvironmentSensor::bme280:
#if FLINT_HAS_BME280
    {
        const auto error = bme.start(at);
        if (error == ESP_OK)
            bmeStatus = ESP_ERR_NOT_FOUND;
        return error;
    }
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    return ESP_ERR_INVALID_ARG;
}
esp_err_t deviceEnvironmentRead(
    EnvironmentSensor sensor, flint::modules::EnvironmentalSample &sample) {
    (void)sample;
    switch (sensor) {
    case EnvironmentSensor::bmp280:
#if FLINT_HAS_BMP280
        sample = bmpStatus == ESP_OK ? bmpSample : flint::modules::EnvironmentalSample{};
        return bmpStatus;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case EnvironmentSensor::bme280:
#if FLINT_HAS_BME280
        sample = bmeStatus == ESP_OK ? bmeSample : flint::modules::EnvironmentalSample{};
        return bmeStatus;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    return ESP_ERR_INVALID_ARG;
}
esp_err_t deviceLightStart() {
#if FLINT_HAS_LIGHT
    const auto error = light.start(uint64_t(esp_timer_get_time() / 1000));
    if (error == ESP_OK)
        lightStatus = ESP_ERR_NOT_FOUND;
    return error;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceLightRead(double &lux) {
    lux = 0;
#if FLINT_HAS_LIGHT
    if (lightStatus == ESP_OK)
        lux = lightLux;
    return lightStatus;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
