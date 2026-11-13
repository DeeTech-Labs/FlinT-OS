#pragma once

/**
 * @brief Измерение напряжения аккумулятора через делитель и ADC1 ESP32-S3.
 * Включение: [modules.battery_adc] с pins.adc и divider=[numerator, denominator].
 * begin(gpio, 3, 1) настраивает ADC и аппаратную curve-fitting калибровку;
 * millivolts(value) усредняет 8 измерений и возвращает напряжение батареи в мВ.
 * Пример: int mv{}; const auto error = battery.millivolts(mv);
 * Без калибровки или при насыщении ADC возвращается ошибка, выдуманного процента нет.
 * Объект владеет ADC unit/calibration и освобождает их в деструкторе. Один владелец,
 * вызовы из задачи Core, не из ISR. В OS чтение: system.power.status("voltage_mv").
 */

#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
namespace flint::modules {
class BatteryAdc {
public:
    ~BatteryAdc();
    BatteryAdc() = default;
    BatteryAdc(const BatteryAdc &) = delete;
    BatteryAdc &operator=(const BatteryAdc &) = delete;
    esp_err_t begin(int gpio, unsigned numerator, unsigned denominator);
    esp_err_t millivolts(int &value);
    bool ready() const {
        return calibration_ != nullptr;
    }

private:
    adc_oneshot_unit_handle_t unit_{};
    adc_cali_handle_t calibration_{};
    adc_channel_t channel_{};
    unsigned numerator_{}, denominator_{};
    void stop();
};
} // namespace flint::modules
