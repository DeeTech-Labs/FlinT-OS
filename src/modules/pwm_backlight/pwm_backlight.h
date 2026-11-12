#pragma once

/**
 * @brief PWM яркость active-high подсветки LCD, частота 5 kHz, аппаратный LEDC.
 * Включение: [modules.pwm_backlight], pins.output. Ресурсы: timer 1 / channel 1,
 * отдельные от зуммера. begin(gpio) держит подсветку выключенной до готовности дисплея.
 * set(percent) принимает 0..100; при 0 timer приостанавливается, при ненулевой яркости
 * возобновляется. Всегда проверяйте esp_err_t.
 * В OS displayBegin включает 100% после заполнения GRAM; ручная регулировка:
 * deviceBacklight(50) из hardware/peripherals.h, из задачи Core.
 * Деструктор выключает канал. Вызовы из ISR и параллельное управление каналом запрещены.
 */

#include "hardware/pwm.h"
namespace flint::modules {
class PwmBacklight {
public:
    esp_err_t begin(int gpio) {
        return output_.begin(gpio, LEDC_TIMER_1, LEDC_CHANNEL_1, 5000);
    }
    esp_err_t set(unsigned percent);
    bool ready() const {
        return output_.ready();
    }

private:
    flint::hardware::PwmOutput output_;
};
} // namespace flint::modules
