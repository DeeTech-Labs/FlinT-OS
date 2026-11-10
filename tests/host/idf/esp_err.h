#pragma once
using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 1,
                           ESP_ERR_INVALID_STATE = 2, ESP_ERR_NO_MEM = 3;
inline const char *esp_err_to_name(esp_err_t) {
    return "test error";
}
