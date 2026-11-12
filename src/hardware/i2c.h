#pragma once
#include "driver/i2c_master.h"
#include <cstddef>
#include <cstdint>

namespace flint::hardware {
// Core-task owned; devices must be destroyed before their shared bus.
class I2cDevice {
public:
    I2cDevice() = default;
    ~I2cDevice();
    I2cDevice(const I2cDevice &) = delete;
    I2cDevice &operator=(const I2cDevice &) = delete;
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address, uint32_t frequency = 400000);
    esp_err_t read(uint8_t reg, uint8_t *data, size_t size) const;
    esp_err_t write(uint8_t reg, uint8_t value) const;
    esp_err_t write(uint8_t reg, const uint8_t *data, size_t size) const;
    void stop();
    bool ready() const {
        return device_ != nullptr;
    }

private:
    i2c_master_dev_handle_t device_{};
};
} // namespace flint::hardware

// One bus for all selected device modules. No driver may delete this handle.
i2c_master_bus_handle_t deviceI2cBus();
