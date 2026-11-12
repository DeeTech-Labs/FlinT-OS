#pragma once
#include "i2c.h"
#include <array>
namespace flint::modules {
struct EnvironmentalSample {
    double temperatureC{}, pressurePa{}, humidityPercent{};
    bool humidityAvailable{};
};
} // namespace flint::modules
namespace flint::hardware {
// Shared register/calibration implementation for BMP280 and BME280; no background task.
class Bosch280 {
public:
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address, bool humidity);
    esp_err_t start(uint64_t nowMs);
    esp_err_t read(modules::EnvironmentalSample &sample, uint64_t nowMs);
    uint64_t nextWake() const {
        return readyAt_;
    }
    bool ready() const {
        return device_.ready();
    }

private:
    I2cDevice device_;
    std::array<int32_t, 12> calibration_{};
    std::array<int32_t, 6> humidityCalibration_{};
    bool humidity_{};
    uint64_t readyAt_{UINT64_MAX};
};
} // namespace flint::hardware
