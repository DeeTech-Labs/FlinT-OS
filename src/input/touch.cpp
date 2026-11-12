#include "touch.h"
#include "board/device_config.h"

#if FLINT_HAS_TOUCH
#include "board/device_touch.h"
#include "core/bridge.h"
#include "cst816.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_state.h"

namespace {
static_assert(
    FLINT_TOUCH_TRANSFORM.width == FLINT_DESIGN_W && FLINT_TOUCH_TRANSFORM.height == FLINT_DESIGN_H,
    "Touch transform must target the selected logical canvas");
flint::input::Cst816 controller;
flint::input::TouchState state;
TaskHandle_t owner{};
uint64_t nextRead = UINT64_MAX;
bool sampling{};
unsigned invalidSamples{};
constexpr uint64_t SampleMs = 16;
uint64_t now() {
    return uint64_t(esp_timer_get_time() / 1000);
}
flint::Error post(void *, flint::InputEvent event) {
    return corePostInput(event);
}
void IRAM_ATTR wake(void *context) {
    BaseType_t higherPriorityWoken = pdFALSE;
    xTaskNotifyFromISR(
        static_cast<TaskHandle_t>(context), CoreWakeTouch, eSetBits, &higherPriorityWoken);
    if (higherPriorityWoken)
        portYIELD_FROM_ISR();
}
} // namespace
bool touchBegin() {
    owner = xTaskGetCurrentTaskHandle();
    const auto error = controller.begin(FLINT_TOUCH_CONFIG, wake, owner);
    if (error != ESP_OK) {
        ESP_LOGE("touch", "initialization: %s", esp_err_to_name(error));
        return false;
    }
    sampling = true;
    nextRead = 0;
    return true;
}
bool touchReady() {
    return controller.ready();
}
void touchMaintain() {
    const auto at = now();
    const auto generation = coreRuntimeGeneration();
    state.flush(generation, at, post, nullptr);
    if (!controller.ready())
        return;
    // Called for an IRQ notification or deadline; other Core work must not poll I2C.
    if (!sampling || at < nextRead)
        return;
    flint::input::TouchSample sample{};
    const auto error = controller.read(sample);
    int x = sample.x, y = sample.y;
    const bool valid = error == ESP_OK && (!sample.down || FLINT_TOUCH_TRANSFORM.map(x, y));
    state.sample(valid, sample.down, x, y, generation, at, post, nullptr);
    invalidSamples = valid ? 0 : invalidSamples + 1;
    if ((error != ESP_OK && error != ESP_ERR_INVALID_RESPONSE) || invalidSamples >= 3) {
        ESP_LOGE("touch", "read: %s; input disabled until restart",
            esp_err_to_name(error == ESP_OK ? ESP_ERR_INVALID_RESPONSE : error));
        controller.stop();
        sampling = false;
    } else
        sampling = sample.down || !valid;
    nextRead = sampling ? at + SampleMs : UINT64_MAX;
}
void touchInterrupt() {
    if (controller.ready()) {
        if (!sampling)
            nextRead = 0;
        sampling = true;
    }
}
uint64_t touchNextWake() {
    return state.pending() ? 0 : nextRead;
}
#else
bool touchBegin() {
    return false;
}
bool touchReady() {
    return false;
}
void touchMaintain() {}
void touchInterrupt() {}
uint64_t touchNextWake() {
    return UINT64_MAX;
}
#endif
