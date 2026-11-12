#pragma once
#include "esp_err.h"
#include <ctime>
#include <flint/core.h>
namespace flint::modules {
struct MotionSample;
struct EnvironmentalSample;
} // namespace flint::modules
// All entry points run on the Core task. No direct Lua/GPIO exposure.
void deviceHardwareBegin();
void deviceHardwareDiagnostics(flint::Values &values);
flint::Error devicePowerStatus(std::string_view key, flint::AppReply &reply);
esp_err_t deviceBacklight(unsigned percent);
esp_err_t deviceTone(unsigned frequency, unsigned durationMs);
void deviceHardwareMaintain();
uint64_t deviceHardwareNextWake();
esp_err_t deviceMotionEnable(bool enabled);
esp_err_t deviceMotionRead(flint::modules::MotionSample &sample);
esp_err_t deviceRtcRead(std::tm &utc);
esp_err_t deviceRtcSet(const std::tm &utc);
// Call only after durable storage and transports have been quiesced.
esp_err_t deviceReleasePower();

// A single conversion is scheduled only when explicitly requested. Results are cached
// after deviceHardwareMaintain(), including automatic shutdown of the light sensor.
enum class EnvironmentSensor { bmp280, bme280 };
esp_err_t deviceEnvironmentStart(EnvironmentSensor sensor);
esp_err_t deviceEnvironmentRead(
    EnvironmentSensor sensor, flint::modules::EnvironmentalSample &sample);
esp_err_t deviceLightStart();
esp_err_t deviceLightRead(double &lux);
