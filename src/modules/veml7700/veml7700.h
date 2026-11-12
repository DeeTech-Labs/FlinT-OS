#pragma once
/**
 * @brief VEML7700: освещённость, фиксированный I2C-адрес 0x10.
 * Включение: [modules.veml7700], pins.sda/scl. begin(bus) оставляет ALS в shutdown.
 * start(nowMs) включает gain=1/8, integration=100ms; read(lux, nowMs) через nextWake()
 * читает ALS и выключает его. До deadline — ESP_ERR_NOT_FOUND; насыщение — ошибка.
 * Пример: double lux{}; const auto error = sensor.read(lux, nowMs); используйте только ESP_OK.
 * Возвращается линейное значение в lux с коэффициентом 0.4608 lx/count; автоматического
 * изменения gain/IT и коррекции нелинейности на высокой освещённости нет.
 * Самостоятельный владелец обязан вызвать read после deadline, чтобы выключить ALS.
 * OS-адаптер делает это в hardware worker даже без запроса результата. Деструктор — shutdown.
 * Нет фоновой задачи; вызовы только из hardware worker, не ISR. Общая I2C-шина не принадлежит
 * модулю.
 */
#include "hardware/i2c.h"
namespace flint::modules {
class Veml7700 {
public:
    ~Veml7700();
    esp_err_t begin(i2c_master_bus_handle_t bus);
    void stop();
    esp_err_t start(uint64_t nowMs);
    esp_err_t read(double &lux, uint64_t nowMs);
    uint64_t nextWake() const {
        return readyAt_;
    }
    bool ready() const {
        return device_.ready();
    }

private:
    flint::hardware::I2cDevice device_;
    uint64_t readyAt_{UINT64_MAX};
    esp_err_t shutdown();
};
} // namespace flint::modules
