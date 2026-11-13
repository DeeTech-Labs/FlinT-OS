#include "bosch280.h"
#include <algorithm>
#include <bit>
#include <cmath>
namespace flint::hardware {
namespace {
uint16_t little(const uint8_t *bytes) {
    return uint16_t(bytes[0] | uint16_t(bytes[1]) << 8);
}
int32_t signed12(unsigned value) {
    return value & 0x800 ? int32_t(value) - 4096 : int32_t(value);
}
} // namespace
esp_err_t Bosch280::begin(i2c_master_bus_handle_t bus, uint8_t address, bool humidity) {
    auto error = beginAsync(bus, address, humidity);
    while (error == ESP_OK && initializing())
        error = maintainInit();
    return error;
}
esp_err_t Bosch280::beginAsync(i2c_master_bus_handle_t bus, uint8_t address, bool humidity) {
    if (address != 0x76 && address != 0x77)
        return ESP_ERR_INVALID_ARG;
    stop();
    const auto error = device_.begin(bus, address);
    if (error == ESP_OK) {
        humidity_ = humidity;
        initStage_ = 1;
    }
    return error;
}
esp_err_t Bosch280::maintainInit() {
    if (!initializing())
        return ESP_ERR_INVALID_STATE;
    esp_err_t error = ESP_OK;
    switch (initStage_) {
    case 1: {
        uint8_t id{};
        error = device_.read(0xd0, &id, 1);
        if (error == ESP_OK && id != (humidity_ ? 0x60 : 0x58))
            error = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    case 2:
        error = device_.write(0xf4, 0);
        break;
    case 3: {
        uint8_t status{};
        error = device_.read(0xf3, &status, 1);
        if (error == ESP_OK && (status & 9))
            error = ESP_ERR_NOT_FOUND;
        break;
    }
    case 4: {
        uint8_t trim[24]{};
        error = device_.read(0x88, trim, sizeof(trim));
        if (error == ESP_OK) {
            for (unsigned i = 0; i < calibration_.size(); ++i) {
                const auto value = little(trim + i * 2);
                calibration_[i] =
                    (i == 0 || i == 3) ? int32_t(value) : std::bit_cast<int16_t>(value);
            }
            if (!calibration_[0] || calibration_[0] == 65535 || !calibration_[3])
                error = ESP_ERR_INVALID_RESPONSE;
        }
        break;
    }
    case 5:
        error = device_.read(0xa1, &humidityTrim1_, 1);
        break;
    case 6: {
        uint8_t h[7]{};
        error = device_.read(0xe1, h, sizeof(h));
        if (error == ESP_OK)
            humidityCalibration_ = {humidityTrim1_, std::bit_cast<int16_t>(little(h)), h[2],
                signed12(unsigned(h[3]) * 16 + (h[4] & 15)),
                signed12(unsigned(h[5]) * 16 + (h[4] >> 4)), std::bit_cast<int8_t>(h[6])};
        break;
    }
    default:
        return ESP_ERR_INVALID_STATE;
    }
    if (error != ESP_OK) {
        stop();
        return error;
    }
    if (++initStage_ > (humidity_ ? 6u : 4u)) {
        initStage_ = 0;
        initialized_ = true;
    }
    return ESP_OK;
}
esp_err_t Bosch280::start(uint64_t nowMs) {
    if (!ready() || readyAt_ != UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    if (nowMs > UINT64_MAX - 12)
        return ESP_ERR_INVALID_ARG;
    auto error = humidity_ ? device_.write(0xf2, 1) : ESP_OK;
    if (error == ESP_OK)
        error = device_.write(0xf4, 0x25); // x1 T/P, forced single conversion.
    if (error == ESP_OK)
        readyAt_ = nowMs + 12;
    return error;
}
esp_err_t Bosch280::read(modules::EnvironmentalSample &sample, uint64_t nowMs) {
    sample = {};
    if (readyAt_ == UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    if (nowMs < readyAt_)
        return ESP_ERR_NOT_FOUND;
    readyAt_ = UINT64_MAX;
    uint8_t status{}, raw[8]{};
    auto error = device_.read(0xf3, &status, 1);
    if (error == ESP_OK && (status & 9))
        error = ESP_ERR_TIMEOUT;
    if (error != ESP_OK) {
        const auto sleep = device_.write(0xf4, 0);
        return sleep == ESP_OK ? error : sleep;
    }
    error = device_.read(0xf7, raw, humidity_ ? 8 : 6);
    if (error != ESP_OK)
        return error;
    const unsigned pressure = unsigned(raw[0]) * 4096 + unsigned(raw[1]) * 16 + (raw[2] >> 4);
    const unsigned temperature = unsigned(raw[3]) * 4096 + unsigned(raw[4]) * 16 + (raw[5] >> 4);
    if (pressure == 0x80000 || temperature == 0x80000)
        return ESP_ERR_INVALID_RESPONSE;
    const auto &c = calibration_;
    // Bosch datasheet compensation equations; use double to avoid signed shifts/overflow.
    const double delta = double(temperature) / 131072.0 - double(c[0]) / 8192.0;
    const double fine =
        (double(temperature) / 16384.0 - double(c[0]) / 1024.0) * c[1] + delta * delta * c[2];
    const double centered = fine / 2.0 - 64000.0;
    const double offset = centered * centered * c[8] / 32768.0 + centered * c[7] * 2.0;
    const double denominator =
        (1.0 + (c[5] * centered * centered / 524288.0 + c[4] * centered) / 17179869184.0) * c[3];
    if (!std::isfinite(denominator) || denominator <= 0)
        return ESP_ERR_INVALID_RESPONSE;
    double pa =
        (1048576.0 - pressure - (offset / 4.0 + c[6] * 65536.0) / 4096.0) * 6250.0 / denominator;
    pa += (c[11] * pa * pa / 2147483648.0 + pa * c[10] / 32768.0 + c[9]) / 16.0;
    modules::EnvironmentalSample result{fine / 5120.0, pa, 0, humidity_};
    if (humidity_) {
        const auto &h = humidityCalibration_;
        const double d = fine - 76800.0;
        double rh = (unsigned(raw[6]) * 256 + raw[7] - (h[3] * 64.0 + h[4] * d / 16384.0)) *
            (h[1] / 65536.0 * (1.0 + h[5] * d / 67108864.0 * (1.0 + h[2] * d / 67108864.0)));
        rh *= 1.0 - h[0] * rh / 524288.0;
        if (!std::isfinite(rh))
            return ESP_ERR_INVALID_RESPONSE;
        result.humidityPercent = std::clamp(rh, 0.0, 100.0);
    }
    if (!std::isfinite(result.temperatureC) || !std::isfinite(pa) || result.temperatureC < -40 ||
        result.temperatureC > 85 || pa < 30000 || pa > 110000)
        return ESP_ERR_INVALID_RESPONSE;
    sample = result;
    return ESP_OK;
}
} // namespace flint::hardware
