#pragma once
#include "esp_err.h"
using adc_oneshot_unit_handle_t = void *;
using adc_unit_t = int;
using adc_channel_t = int;
inline constexpr int ADC_UNIT_1 = 1, ADC_ATTEN_DB_12 = 12, ADC_BITWIDTH_DEFAULT = 12;
struct adc_oneshot_unit_init_cfg_t {
    int unit_id{};
};
struct adc_oneshot_chan_cfg_t {
    int atten{}, bitwidth{};
};
esp_err_t adc_oneshot_io_to_channel(int, int *, int *);
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *, void **);
esp_err_t adc_oneshot_config_channel(void *, int, const adc_oneshot_chan_cfg_t *);
esp_err_t adc_oneshot_del_unit(void *);
esp_err_t adc_oneshot_read(void *, int, int *);
