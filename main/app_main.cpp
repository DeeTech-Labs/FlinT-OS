#include "esp_err.h"
#include "nvs_flash.h"

void appSetup(bool trustReady);
void appLoop();

extern "C" void app_main() {
    // NVS contains trust keys. Failure leaves USB recovery available; never erase it automatically.
    const bool trustReady = nvs_flash_init() == ESP_OK;
    appSetup(trustReady);
    for (;;) {
        appLoop();
    }
}
