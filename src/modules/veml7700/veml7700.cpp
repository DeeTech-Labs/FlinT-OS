#include "veml7700.h"
#include "esp_log.h"
namespace flint::modules {
esp_err_t Veml7700::shutdown() {
    const uint8_t config[]{0x01, 0x10};
    return device_.write(0, config, sizeof(config));
}
Veml7700::~Veml7700() {
    if (ready() && shutdown() != ESP_OK)
        ESP_LOGE("light", "shutdown failed");
}
void Veml7700::stop() {
    if (ready())
        shutdown();
    device_.stop();
    readyAt_ = UINT64_MAX;
}
esp_err_t Veml7700::begin(i2c_master_bus_handle_t bus) {
    stop();
    auto error = device_.begin(bus, 0x10);
    if (error == ESP_OK)
        error = shutdown();
    if (error != ESP_OK)
        device_.stop();
    return error;
}
esp_err_t Veml7700::start(uint64_t nowMs) {
    if (!ready() || readyAt_ != UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    if (nowMs > UINT64_MAX - 210)
        return ESP_ERR_INVALID_ARG;
    const uint8_t config[]{0x00, 0x10}; // Little-endian gain=1/8, IT=100ms, enabled.
    const auto error = device_.write(0, config, sizeof(config));
    if (error == ESP_OK)
        readyAt_ = nowMs + 210; // Two integrations plus oscillator startup margin.
    else if (shutdown() != ESP_OK)
        ESP_LOGE("light", "failed start cleanup failed");
    return error;
}
esp_err_t Veml7700::read(double &lux, uint64_t nowMs) {
    lux = 0;
    if (readyAt_ == UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    if (nowMs < readyAt_)
        return ESP_ERR_NOT_FOUND;
    readyAt_ = UINT64_MAX;
    uint8_t bytes[2]{};
    auto error = device_.read(4, bytes, sizeof(bytes));
    const auto off = shutdown();
    if (off != ESP_OK)
        return off;
    if (error != ESP_OK)
        return error;
    const auto raw = uint16_t(bytes[0] | uint16_t(bytes[1]) << 8);
    if (raw == 65535)
        return ESP_ERR_INVALID_RESPONSE;
    lux = raw * 0.4608;
    return ESP_OK;
}
} // namespace flint::modules
