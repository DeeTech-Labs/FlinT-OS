#pragma once

/**
 * @brief Пассивный зуммер с active-high драйвером: генерация тона аппаратным LEDC.
 * Включение: [modules.pwm_buzzer], pins.output. Ресурсы: LEDC timer 0 / channel 0.
 * begin(gpio) оставляет выход выключенным. tone(Hz, durationMs, nowMs) принимает
 * 100..10000 Hz и 0..5000 мс; durationMs=0 выключает звук. Ошибки проверять по ESP_OK.
 * Владелец включает nextWake() в свой deadline и вызывает maintain(nowMs) после пробуждения;
 * точность остановки ограничена задержкой event loop. Новая задача/таймер RTOS не создаётся.
 * В OS это уже подключено к Core: deviceTone(2000, 100) из hardware/peripherals.h.
 * В idle LEDC timer приостановлен; деструктор выключает канал. Только Core, не ISR.
 */

#include "hardware/pwm.h"
#include <cstdint>
namespace flint::modules {
class PwmBuzzer {
public:
    esp_err_t begin(int gpio) {
        return output_.begin(gpio, LEDC_TIMER_0, LEDC_CHANNEL_0, 2000);
    }
    esp_err_t tone(unsigned frequency, unsigned durationMs, uint64_t now);
    esp_err_t maintain(uint64_t now);
    uint64_t nextWake() const {
        return stopAt_;
    }
    bool ready() const {
        return output_.ready();
    }

private:
    flint::hardware::PwmOutput output_;
    uint64_t stopAt_{UINT64_MAX};
};
} // namespace flint::modules
