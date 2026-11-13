#include "input/touch.h"
#include "board/device_config.h"
#include "board/device_touch.h"
#include "core/bridge.h"
#include "cst816.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware/i2c.h"
#include "hardware/peripherals.h"
#include "hardware/touch_records.h"
#include "input/touch_state.h"
#include <array>
namespace {
static_assert(FLINT_TOUCH_TRANSFORM.width == FLINT_DESIGN_W &&
    FLINT_TOUCH_TRANSFORM.height == FLINT_DESIGN_H);
flint::modules::Cst816 controller;
flint::input::TouchState state;
using Raw = flint::hardware::RawTouch;
flint::hardware::TouchRecords records;
bool ready{}, sampling{};
uint64_t generation{}, nextRead = UINT64_MAX;
unsigned invalidSamples{};
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
flint::Error post(void *, flint::InputEvent event) {
    return corePostInput(event);
}
void IRAM_ATTR wake(void *) {
    deviceHardwareWakeFromIsr();
}
} // namespace
// Hardware-worker entry points never invoke Core or Lua.
bool touchWorkerBegin() {
    const auto error = controller.beginAsync(
        FLINT_TOUCH_CONFIG, wake, nullptr, deviceI2cBus(), uint64_t(esp_timer_get_time() / 1000));
    portENTER_CRITICAL(&lock);
    ready = false;
    portEXIT_CRITICAL(&lock);
    sampling = false;
    nextRead = error == ESP_OK ? controller.nextInitWake() : UINT64_MAX;
    return error == ESP_OK;
}
void touchWorkerInterrupt() {
    if (controller.ready()) {
        sampling = true;
        nextRead = 0;
    }
}
uint64_t touchWorkerNextWake() {
    return nextRead;
}
void touchWorkerMaintain(uint64_t at) {
    if (controller.nextInitWake() != UINT64_MAX) {
        if (at >= controller.nextInitWake()) {
            controller.maintainInit(at);
            portENTER_CRITICAL(&lock);
            ready = controller.ready();
            portEXIT_CRITICAL(&lock);
        }
        nextRead = controller.nextInitWake();
        if (!controller.ready())
            return;
        sampling = true;
        nextRead = 0;
    }
    if (!sampling || at < nextRead)
        return;
    portENTER_CRITICAL(&lock);
    const auto acquisitionGeneration = generation;
    portEXIT_CRITICAL(&lock);
    flint::modules::TouchSample sample{};
    const auto error = controller.read(sample);
    int x = sample.x, y = sample.y;
    const bool valid = error == ESP_OK && (!sample.down || FLINT_TOUCH_TRANSFORM.map(x, y));
    portENTER_CRITICAL(&lock);
    records.push({at, acquisitionGeneration, x, y, valid, sample.down});
    portEXIT_CRITICAL(&lock);
    coreWake(CoreWakeWork); // Notification only: raw input is interpreted on Core.
    invalidSamples = valid ? 0 : invalidSamples + 1;
    if ((error != ESP_OK && error != ESP_ERR_INVALID_RESPONSE) || invalidSamples >= 3) {
        controller.stop();
        sampling = false;
        portENTER_CRITICAL(&lock);
        ready = false;
        portEXIT_CRITICAL(&lock);
    } else
        sampling = sample.down || !valid;
    nextRead = sampling ? at + 16 : UINT64_MAX;
}
bool touchBegin() {
    return touchReady();
}
bool touchReady() {
    portENTER_CRITICAL(&lock);
    const bool value = ready;
    portEXIT_CRITICAL(&lock);
    return value;
}
void touchMaintain() {
    const auto at = uint64_t(esp_timer_get_time() / 1000);
    const auto current = coreRuntimeGeneration();
    std::array<Raw, 16> batch{};
    size_t size{};
    bool lost{};
    portENTER_CRITICAL(&lock);
    generation = current;
    size = records.drain(batch, lost);
    portEXIT_CRITICAL(&lock);
    state.flush(current, at, post, nullptr);
    if (lost)
        state.sample(false, false, 0, 0, current, at, post, nullptr);
    for (size_t i = 0; i < size; ++i) {
        const auto &record = batch[i];
        if (record.generation != current) {
            state.sample(false, false, 0, 0, current, at, post, nullptr);
            continue;
        }
        state.sample(
            record.valid, record.down, record.x, record.y, current, record.at, post, nullptr);
    }
}
void touchInterrupt() {} // IRQ is owned by the hardware worker.
uint64_t touchNextWake() {
    return state.pending() ? 0 : UINT64_MAX;
}
