#pragma once

/**
 * QMI8658C, I2C0x6b. begin checks WHO_AM_I and keeps sensors powered down.
 * enable(true, intervalMs) selects equal accel/gyro ODR code7(<36ms) or8 otherwise.
 * Raw acceleration /16384=g, angularVelocity /64=degrees/s; read requires data-ready.
 * Fixed sensor-aligned X/Y/Z, no display rotation or verified mounting calibration.
 * The OS hardware worker owns the driver; public SI samples use sensors::Broker.
 * No fusion, wake-on-motion or IMU INT handler. The shared bus is never owned here.
 */

#include "hardware/i2c.h"
#include <array>
namespace flint::modules {
struct MotionSample {
    std::array<int16_t, 3> acceleration{}, angularVelocity{};
};
// Raw signed samples: 16384 LSB/g and 64 LSB/(degree/s), ODR selected from 56.05/28.025 Hz 6DOF
// rates. Sensors stay powered down until explicitly enabled. No background polling.
class Qmi8658 {
public:
    ~Qmi8658();
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address = 0x6b);
    void stop();
    esp_err_t enable(bool enabled, uint32_t intervalMs = 40);
    esp_err_t read(MotionSample &sample);
    bool ready() const {
        return device_.ready();
    }

private:
    flint::hardware::I2cDevice device_;
    bool enabled_{};
};
} // namespace flint::modules
