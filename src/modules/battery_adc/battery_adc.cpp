#include "battery_adc.h"
#include "esp_log.h"
namespace flint::modules {
BatteryAdc::~BatteryAdc() {
    stop();
}
void BatteryAdc::stop() {
    if (calibration_ && adc_cali_delete_scheme_curve_fitting(calibration_) != ESP_OK)
        ESP_LOGE("battery", "calibration cleanup failed");
    calibration_ = nullptr;
    if (unit_ && adc_oneshot_del_unit(unit_) != ESP_OK)
        ESP_LOGE("battery", "ADC cleanup failed");
    unit_ = nullptr;
}
esp_err_t BatteryAdc::begin(int gpio, unsigned numerator, unsigned denominator) {
    if (unit_)
        return ESP_ERR_INVALID_STATE;
    if (!denominator || numerator < denominator || numerator > 100 || denominator > 100)
        return ESP_ERR_INVALID_ARG;
    adc_unit_t number{};
    auto error = adc_oneshot_io_to_channel(gpio, &number, &channel_);
    if (error != ESP_OK || number != ADC_UNIT_1)
        return ESP_ERR_INVALID_ARG;
    adc_oneshot_unit_init_cfg_t config{};
    config.unit_id = number;
    error = adc_oneshot_new_unit(&config, &unit_);
    adc_oneshot_chan_cfg_t channel{};
    channel.atten = ADC_ATTEN_DB_12;
    channel.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (error == ESP_OK)
        error = adc_oneshot_config_channel(unit_, channel_, &channel);
    adc_cali_curve_fitting_config_t calibration{};
    calibration.unit_id = number;
    calibration.chan = channel_;
    calibration.atten = channel.atten;
    calibration.bitwidth = channel.bitwidth;
    if (error == ESP_OK)
        error = adc_cali_create_scheme_curve_fitting(&calibration, &calibration_);
    if (error != ESP_OK) {
        stop();
        return error;
    }
    numerator_ = numerator;
    denominator_ = denominator;
    return ESP_OK;
}
esp_err_t BatteryAdc::millivolts(int &value) {
    value = 0;
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    int sum = 0;
    for (unsigned i = 0; i < 8; ++i) {
        int raw{}, mv{};
        auto error = adc_oneshot_read(unit_, channel_, &raw);
        if (error == ESP_OK)
            error = adc_cali_raw_to_voltage(calibration_, raw, &mv);
        if (error != ESP_OK)
            return error;
        if (raw == 4095 || mv < 0 || mv > 3300)
            return ESP_ERR_INVALID_RESPONSE;
        sum += mv;
    }
    value = int((int64_t(sum) * numerator_ + 4 * denominator_) / (8 * denominator_));
    return ESP_OK;
}
} // namespace flint::modules
