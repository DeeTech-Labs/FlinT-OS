#pragma once

/**
 * @brief CST816T/S: один контакт touch, raw X/Y и GPIO-прерывание касания.
 * Включение: [modules.cst816] в device.toml; GPIO, raw_size и swap/mirror задаются там.
 * Порядок: begin(config, wakeFromIsr, context, sharedBus), затем read(sample), stop().
 * Проверяйте ESP_OK; sample.down=false означает отпускание. Преобразование координат
 * в canvas выполняет TouchTransform, а не этот драйвер.
 * Пример чтения после begin: TouchSample s{}; const auto error = touch.read(s);
 * wakeFromIsr выполняется в ISR: только уведомить Core, без I2C и обращения к UI.
 * Переданный sharedBus не принадлежит драйверу; без него драйвер создаёт собственный.
 * Объект живёт до stop(), владеет своим device и GPIO handler; деструктор вызывает stop().
 * В OS используйте input/touch.h: адаптер сам разделяет шину с RTC/IMU и отменяет жесты.
 */

#include "driver/i2c_master.h"
#include "esp_attr.h"
#include <cstdint>

namespace flint::modules {
struct TouchSample {
    bool down{};
    uint16_t x{}, y{};
};

struct Cst816Config {
    int port{-1}, sda{-1}, scl{-1}, reset{-1}, interrupt{-1};
    uint32_t frequency{400000};
    uint8_t address{0x15};
};

// Core-task owned. The callback executes in GPIO ISR context and must only wake
// the owner; never perform I2C or access Core there. Keep this object alive until stop().
class Cst816 {
public:
    Cst816() = default;
    ~Cst816();
    Cst816(const Cst816 &) = delete;
    Cst816 &operator=(const Cst816 &) = delete;

    esp_err_t begin(const Cst816Config &config, void (*wakeFromIsr)(void *), void *context,
        i2c_master_bus_handle_t sharedBus = nullptr);
    esp_err_t read(TouchSample &sample);
    void stop();
    bool ready() const {
        return ready_;
    }

private:
    i2c_master_bus_handle_t bus_{};
    i2c_master_dev_handle_t device_{};
    bool busOwned_{}, handlerInstalled_{}, ready_{};
    int interrupt_{-1};
    void (*wake_)(void *){};
    void *context_{};
    static void IRAM_ATTR interrupt(void *context);
};
} // namespace flint::modules
