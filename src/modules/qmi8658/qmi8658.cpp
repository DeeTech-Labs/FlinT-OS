#include "qmi8658.h"
#include "esp_log.h"
#include <bit>
namespace flint::modules {
Qmi8658::~Qmi8658() {
    if (ready() && enable(false) != ESP_OK)
        ESP_LOGE("imu", "power-down failed");
}
esp_err_t Qmi8658::begin(i2c_master_bus_handle_t bus, uint8_t address) {
    auto error = device_.begin(bus, address);
    uint8_t id{};
    if (error == ESP_OK)
        error = device_.read(0, &id, 1);
    if (error == ESP_OK && id != 5)
        error = ESP_ERR_NOT_SUPPORTED;
    if (error == ESP_OK)
        error = device_.write(8, 0); // Disable all sensors.
    if (error == ESP_OK)
        error = device_.write(2, 0x41); // Auto increment, powered down.
    if (error == ESP_OK)
        error = device_.write(3, 0x06); // +/-2g, 125Hz.
    if (error == ESP_OK)
        error = device_.write(4, 0x56); // +/-512dps, 125Hz.
    if (error != ESP_OK)
        device_.stop();
    return error;
}
esp_err_t Qmi8658::enable(bool enabled) {
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    auto error = device_.write(8, 0);
    if (error == ESP_OK)
        error = device_.write(2, enabled ? 0x40 : 0x41);
    if (error == ESP_OK && enabled)
        error = device_.write(8, 0x83); // Synchronized sample locking.
    enabled_ = enabled && error == ESP_OK;
    if (error != ESP_OK) {
        const auto disable = device_.write(8, 0);
        const auto sleep = device_.write(2, 0x41);
        if (disable != ESP_OK || sleep != ESP_OK)
            ESP_LOGE("imu", "failure cleanup failed");
    }
    return error;
}
esp_err_t Qmi8658::read(MotionSample &sample) {
    sample = {};
    if (!enabled_)
        return ESP_ERR_INVALID_STATE;
    uint8_t status{};
    auto error = device_.read(0x2e, &status, 1);
    if (error != ESP_OK)
        return error;
    if ((status & 3) != 3)
        return ESP_ERR_NOT_FOUND;
    uint8_t raw[12]{};
    error = device_.read(0x35, raw, sizeof(raw));
    if (error != ESP_OK)
        return error;
    for (unsigned i = 0; i < 6; ++i) {
        const auto value =
            std::bit_cast<int16_t>(uint16_t(raw[i * 2] | uint16_t(raw[i * 2 + 1]) << 8));
        (i < 3 ? sample.acceleration[i] : sample.angularVelocity[i - 3]) = value;
    }
    return ESP_OK;
}
} // namespace flint::modules
