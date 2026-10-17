/*
 * FlinT OS — ESP-IDF entry (ESP32-C3)
 *
 * Product code: components/flint/
 */

#include "app/app.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char* TAG = "main";

extern "C" void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_LOGI(TAG, "FlinT OS starting");
  appSetup();

  for (;;) {
    appLoop();
  }
}
