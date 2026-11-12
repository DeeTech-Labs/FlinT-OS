#include "i2c.h"
#include "board/device_config.h"
#include "esp_log.h"
#include <algorithm>
#include <array>

namespace {
// Process-lifetime bus: never destroyed before clients in other translation units.
// Reboots reset the peripheral; each module owns and releases only its device handle.
i2c_master_bus_handle_t sharedBus{};
void (*serviceHook)(){};
uint64_t (*microsecondsHook)(){};
DeviceI2cMetrics metrics;
uint64_t beforeTransfer() {
    if (serviceHook)
        serviceHook();
    return microsecondsHook ? microsecondsHook() : 0;
}
esp_err_t afterTransfer(esp_err_t error, uint64_t start) {
    const auto elapsed = microsecondsHook ? microsecondsHook() - start : 0;
    ++metrics.transfers;
    metrics.errors += error != ESP_OK;
    metrics.totalTransferUs += elapsed;
    metrics.maxTransferUs = std::max(metrics.maxTransferUs, elapsed);
    return error;
}
} // namespace
i2c_master_bus_handle_t deviceI2cBus() {
    if (!sharedBus) {
        i2c_master_bus_config_t config{};
        config.i2c_port = -1;
        config.sda_io_num = gpio_num_t(FLINT_I2C_SDA);
        config.scl_io_num = gpio_num_t(FLINT_I2C_SCL);
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7;
        const auto error = i2c_new_master_bus(&config, &sharedBus);
        if (error != ESP_OK) {
            ESP_LOGE("i2c", "initialization: %s", esp_err_to_name(error));
            return nullptr;
        }
    }
    return sharedBus;
}
namespace flint::hardware {
I2cDevice::~I2cDevice() {
    stop();
}
void I2cDevice::stop() {
    if (device_ && i2c_master_bus_rm_device(device_) != ESP_OK)
        ESP_LOGE("i2c", "device cleanup failed");
    device_ = nullptr;
}
esp_err_t I2cDevice::begin(i2c_master_bus_handle_t bus, uint8_t address, uint32_t frequency) {
    if (ready())
        return ESP_ERR_INVALID_STATE;
    if (!bus || address < 8 || address > 0x77 || frequency < 10000 || frequency > 400000)
        return ESP_ERR_INVALID_ARG;
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = frequency;
    return i2c_master_bus_add_device(bus, &config, &device_);
}
esp_err_t I2cDevice::read(uint8_t reg, uint8_t *data, size_t size) const {
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    if (!data || !size)
        return ESP_ERR_INVALID_ARG;
    const auto start = beforeTransfer();
    return afterTransfer(i2c_master_transmit_receive(device_, &reg, 1, data, size, 5), start);
}
esp_err_t I2cDevice::write(uint8_t reg, uint8_t value) const {
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    return write(reg, &value, 1);
}
esp_err_t I2cDevice::write(uint8_t reg, const uint8_t *data, size_t size) const {
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    if (!data || !size || size > 16)
        return ESP_ERR_INVALID_ARG;
    std::array<uint8_t, 17> bytes{};
    bytes[0] = reg;
    std::copy_n(data, size, bytes.data() + 1);
    const auto start = beforeTransfer();
    return afterTransfer(i2c_master_transmit(device_, bytes.data(), size + 1, 5), start);
}
} // namespace flint::hardware

void deviceI2cWorkerHooks(void (*service)(), uint64_t (*microseconds)()) {
    serviceHook = service;
    microsecondsHook = microseconds;
}
DeviceI2cMetrics deviceI2cMetrics() {
    return metrics;
}
