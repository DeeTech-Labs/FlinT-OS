#pragma once
/**
 * @brief Bme280: температура, давление и относительная влажность, I2C 0x76/0x77.
 * Включение: [modules.bme280], pins.sda/scl и address. Общая шина не принадлежит модулю.
 * begin(bus, address) читает заводскую калибровку и оставляет датчик в sleep.
 * start(nowMs) запускает один forced sample x1. Через nextWake() вызовите read(sample, nowMs).
 * До готовности возвращается ESP_ERR_NOT_FOUND, повторный start до read запрещён.
 * Пример: flint::modules::EnvironmentalSample s{}; const auto error = sensor.read(s, nowMs);
 * Единицы: temperatureC — °C, pressurePa — Pa, humidityPercent — %RH только у BME280;
 * BMP280 всегда выставляет humidityAvailable=false. Проверяйте ESP_OK перед использованием.
 * Нет polling, высоты или автоматического sea-level correction. Только Core, не ISR.
 * Класс использует общую Bosch280-реализацию компенсации; свой I2C device освобождается RAII.
 */
#include "hardware/bosch280.h"
namespace flint::modules {
class Bme280 {
public:
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address = 0x76) {
        return sensor_.begin(bus, address, true);
    }
    esp_err_t start(uint64_t nowMs) {
        return sensor_.start(nowMs);
    }
    esp_err_t read(EnvironmentalSample &sample, uint64_t nowMs) {
        return sensor_.read(sample, nowMs);
    }
    uint64_t nextWake() const {
        return sensor_.nextWake();
    }
    bool ready() const {
        return sensor_.ready();
    }

private:
    flint::hardware::Bosch280 sensor_;
};
} // namespace flint::modules
