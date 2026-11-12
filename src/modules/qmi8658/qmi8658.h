#pragma once

/**
 * @brief QMI8658C: трёхосевой акселерометр и трёхосевой гироскоп, I2C 0x6b.
 * Включение: [modules.qmi8658] с общей I2C-шиной и опциональным GPIO interrupt.
 * begin(sharedBus) проверяет WHO_AM_I, задаёт диапазоны и оставляет датчики выключенными.
 * Порядок: enable(true), дождаться готовности без busy-wait, read(sample), enable(false).
 * ESP_ERR_NOT_FOUND означает, что полного нового sample ещё нет; не используйте sample.
 * Единицы raw: acceleration / 16384 = g, angularVelocity / 64 = градусов/с, ODR 125 Hz.
 * В OS используйте deviceMotionEnable/Read из hardware/peripherals.h;
 * пример: MotionSample s{}; const auto error = deviceMotionRead(s);
 * Автоматического polling, sensor fusion и wake-on-motion нет. INT резервируется,
 * но обработчик не устанавливается. Общая шина не принадлежит драйверу; свой device
 * освобождается автоматически, датчики выключаются в деструкторе. Только задача Core.
 */

#include "hardware/i2c.h"
#include <array>
namespace flint::modules {
struct MotionSample {
    std::array<int16_t, 3> acceleration{}, angularVelocity{};
};
// Raw signed samples: 16384 LSB/g and 64 LSB/(degree/s), fixed 125 Hz.
// Sensors stay powered down until explicitly enabled. No background polling.
class Qmi8658 {
public:
    ~Qmi8658();
    esp_err_t begin(i2c_master_bus_handle_t bus, uint8_t address = 0x6b);
    esp_err_t enable(bool enabled);
    esp_err_t read(MotionSample &sample);
    bool ready() const {
        return device_.ready();
    }

private:
    flint::hardware::I2cDevice device_;
    bool enabled_{};
};
} // namespace flint::modules
