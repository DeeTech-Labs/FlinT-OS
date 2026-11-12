#pragma once

/**
 * @brief PCF85063A: аппаратный календарь/часы с резервным питанием, I2C 0x51.
 * Включение: [modules.pcf85063], общие pins.sda/scl, опциональный pins.interrupt.
 * begin(sharedBus) проверяет доступность; read(utc) и set(utc) работают со std::tm:
 * tm_year = год - 1900, tm_mon = месяц - 1, tm_wday = 0..6, время — UTC, 2000..2099.
 * Пример OS: std::tm utc{}; const auto error = deviceRtcRead(utc);
 * В OS функции deviceRtcRead/Set объявлены в hardware/peripherals.h; второй экземпляр
 * драйвера на тот же адрес не нужен. Нет автоматической записи RTC при загрузке.
 * Oscillator-stop, STOP и неверный BCD/календарь дают ошибку, а не достоверное время.
 * Неудачный set оставляет STOP: повторный успешный set восстанавливает валидные часы.
 * Владеет своим I2C device, но не общей шиной. Один Core-владелец, вызовы не из ISR.
 * Будильники и wake по RTC INT не реализованы; GPIO INT только резервируется конфигом.
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
