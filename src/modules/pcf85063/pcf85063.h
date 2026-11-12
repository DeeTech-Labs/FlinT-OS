#pragma once

/**
 * PCF85063 RTC UTC, I2C0x51, years2000..2099. Rejects oscillator-stop and malformed BCD.
 * Failed writes retain STOP so partially written UTC cannot be trusted.
 * Only the hardware worker accesses I2C; Core uses deviceRtcReadEpoch/Request/Result
 * cache and asynchronous verified write acknowledgment. No alarm/wakeup handler.
 */

#include "hardware/i2c.h"
#include <ctime>
namespace flint::modules {
class Pcf85063 {
public:
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address = 0x51);
    esp_err_t read(std::tm &utc);
    esp_err_t set(const std::tm &utc);
    bool ready() const {
        return device_.ready();
    }

private:
    flint::hardware::I2cDevice device_;
};
} // namespace flint::modules
