#pragma once
#include "esp_err.h"
using adc_cali_handle_t = void *;
struct adc_cali_curve_fitting_config_t {
    int unit_id{}, chan{}, atten{}, bitwidth{};
};
esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *, void **);
esp_err_t adc_cali_delete_scheme_curve_fitting(void *);
esp_err_t adc_cali_raw_to_voltage(void *, int, int *);
