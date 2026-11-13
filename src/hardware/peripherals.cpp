#include "peripherals.h"
#include "board/device_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#if FLINT_HAS_I2C
#include "i2c.h"
#endif
#if FLINT_HAS_RTC
#include "modules/pcf85063/pcf85063.h"
#endif
#if FLINT_HAS_IMU
#include "modules/qmi8658/qmi8658.h"
#endif
#if FLINT_HAS_BATTERY
#include "modules/battery_adc/battery_adc.h"
#endif
#if FLINT_HAS_BUZZER
#include "modules/pwm_buzzer/pwm_buzzer.h"
#endif
#if FLINT_HAS_BACKLIGHT
#include "modules/pwm_backlight/pwm_backlight.h"
#endif
#if FLINT_HAS_POWER_BUTTON
#include "driver/gpio.h"
#include "hardware/button_state.h"
#include "modules/power_button/power_button.h"
#endif
#if FLINT_HAS_BMP280
#include "modules/bmp280/bmp280.h"
#endif
#if FLINT_HAS_BME280
#include "modules/bme280/bme280.h"
#endif
#if FLINT_HAS_LIGHT
#include "modules/veml7700/veml7700.h"
#endif
#include "core/bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware/rtc_time.h"
#include "hardware/sensor_mailbox.h"
#include "input/touch.h"
#include <algorithm>
#include <array>
#include <atomic>
namespace {
#if FLINT_HAS_RTC
flint::modules::Pcf85063 rtc;
#endif
#if FLINT_HAS_IMU
flint::modules::Qmi8658 imu;
#endif
#if FLINT_HAS_BATTERY
flint::modules::BatteryAdc battery;
#endif
#if FLINT_HAS_BUZZER
flint::modules::PwmBuzzer buzzer;
#endif
#if FLINT_HAS_BACKLIGHT
flint::modules::PwmBacklight backlight;
#endif
#if FLINT_HAS_POWER_BUTTON
flint::modules::PowerButton power;
flint::hardware::ButtonState powerState, bootState;
TaskHandle_t buttonOwner{};
void IRAM_ATTR buttonIrq(void *) {
    BaseType_t higherPriorityWoken = pdFALSE;
    xTaskNotifyFromISR(buttonOwner, CoreWakeButtons, eSetBits, &higherPriorityWoken);
    if (higherPriorityWoken)
        portYIELD_FROM_ISR();
}
flint::Error buttonPost(void *, flint::InputEvent event) {
    return corePostInput(event);
}
#endif
#if FLINT_HAS_BMP280
flint::modules::Bmp280 bmp;

#endif
#if FLINT_HAS_BME280
flint::modules::Bme280 bme;

#endif
#if FLINT_HAS_LIGHT
flint::modules::Veml7700 light;

#endif

bool muted{};
[[maybe_unused]] void report(const char *module, esp_err_t error) {
    if (error != ESP_OK)
        ESP_LOGE("hardware", "%s: %s", module, esp_err_to_name(error));
}
[[maybe_unused]] uint64_t now() {
    return uint64_t(esp_timer_get_time() / 1000);
}
#if FLINT_HAS_I2C || FLINT_HAS_BATTERY
TaskHandle_t worker{};
std::atomic<bool> touchIrqPending{};
portMUX_TYPE mailboxLock = portMUX_INITIALIZER_UNLOCKED;
flint::hardware::SensorMailbox mailbox;
std::array<flint::sensors::Demand, flint::sensors::MaxSources> desired{};
struct Schedule {
    uint64_t next{}, refresh{}, serial{}, lastAt{}, lastGood{}, readyAt{}, generation{},
        recoverAt{};
    uint32_t interval{};
    bool enabled{}, initialized{}, initializing{}, measured{};
    unsigned attempts{};
    uint64_t failedRefresh{};
};
std::array<Schedule, 5> schedules{};
std::tm rtcCache{}, rtcDesired{};
esp_err_t rtcStatus = ESP_ERR_INVALID_STATE;
bool rtcSetPending{}, rtcWriting{};
uint64_t rtcTicket{}, rtcCompleted{}, rtcAt{};
esp_err_t rtcWriteStatus = ESP_ERR_INVALID_STATE;
int batteryMv{};
bool batteryMeasured{};
esp_err_t batteryStatus = ESP_ERR_INVALID_STATE;
uint64_t batteryNext{}, batteryAt{}, rtcNext{};
struct WorkerMetrics {
    uint64_t loops{}, timedWakes{}, notifications{}, missedDeadlines{}, maxWakeDelayMs{},
        maxCycleUs{}, maxGapMs{}, coalesced{}, adcReads{}, adcErrors{}, maxAdcUs{}, i2cTransfers{},
        i2cErrors{}, maxI2cUs{}, i2cTotalUs{}, stackBytes{};
} workerMetrics, metricsCache;
void stopSource(unsigned id) {
    switch (id) {
#if FLINT_HAS_BMP280
    case 1:
        bmp.stop();
        break;
#endif
#if FLINT_HAS_BME280
    case 2:
        bme.stop();
        break;
#endif
#if FLINT_HAS_LIGHT
    case 3:
        light.stop();
        break;
#endif
#if FLINT_HAS_IMU
    case 4:
        imu.stop();
        break;
#endif
    default:
        break;
    }
}
void wakeWorker() {
    if (worker)
        xTaskNotify(worker, 1, eSetBits);
}
size_t catalog(std::span<flint::sensors::Source> output) {
    using namespace flint::sensors;
    size_t count{};
    auto add = [&](Source source) {
        if (count < output.size())
            output[count++] = source;
    };
#if FLINT_HAS_BMP280
    add({1, Mask(bit(Channel::temperature) | bit(Channel::pressure)), 1000, 5000, 60000, 15000, 1,
        false, TemperatureKind::ambient});
#endif
#if FLINT_HAS_BME280
    add({2, Mask(bit(Channel::temperature) | bit(Channel::pressure) | bit(Channel::humidity)), 1000,
        5000, 60000, 15000, 0, false, TemperatureKind::ambient});
#endif
#if FLINT_HAS_LIGHT
    add({3, bit(Channel::ambient_light), 500, 1000, 60000, 5000});
#endif
#if FLINT_HAS_IMU
    add({4, Mask(bit(Channel::acceleration) | bit(Channel::angular_velocity)), 20, 40, 60000, 200,
        0, false, TemperatureKind::unspecified, Coordinates::device, 19.6133, 8.936085770210967,
        false});
#endif
    (void)add;
    return count;
}
class Backend final : public flint::sensors::Backend {
public:
    size_t catalog(std::span<flint::sensors::Source> output) override {
        return ::catalog(output);
    }
    void demand(std::span<const flint::sensors::Demand> input) override {
        portENTER_CRITICAL(&mailboxLock);
        mailbox.demand(input);
        portEXIT_CRITICAL(&mailboxLock);
        wakeWorker();
    }
    size_t poll(uint64_t, std::span<flint::sensors::Sample> output) override {
        portENTER_CRITICAL(&mailboxLock);
        auto count = mailbox.poll(output);
        portEXIT_CRITICAL(&mailboxLock);
        return count;
    }
    uint64_t nextWake() const override {
        return UINT64_MAX;
    }
} backend;
void publish(unsigned id, flint::sensors::Mask mask, esp_err_t error,
    const std::array<flint::sensors::Vector3, 6> &values, uint64_t at,
    flint::sensors::Status status = flint::sensors::Status::valid) {
    flint::sensors::Sample sample{};
    sample.source = id;
    sample.channels = mask;
    sample.generation = schedules[id].generation;
    sample.sampleId = ++schedules[id].serial;
    sample.at = at;
    if (status == flint::sensors::Status::valid) {
        schedules[id].lastAt = at;
        schedules[id].measured = true;
    }
    sample.status = error == ESP_OK ? status : flint::sensors::Status::failed;
    sample.reason = error == ESP_OK && status == flint::sensors::Status::valid ? flint::Error::ok
        : error == ESP_ERR_INVALID_RESPONSE || error == ESP_ERR_INVALID_ARG
        ? flint::Error::invalid_argument
        : error == ESP_OK || error == ESP_ERR_NOT_FOUND || error == ESP_ERR_NOT_SUPPORTED ||
            error == ESP_ERR_INVALID_STATE
        ? flint::Error::source_unavailable
        : flint::Error::io_error;
    if (error == ESP_OK && status == flint::sensors::Status::valid) {
        sample.values = values;
        schedules[id].attempts = 0;
        if (schedules[id].lastGood)
            workerMetrics.maxGapMs = std::max(workerMetrics.maxGapMs, at - schedules[id].lastGood);
        schedules[id].lastGood = at;
    }
    if (error != ESP_OK) {
        auto &schedule = schedules[id];
        schedule.initialized = schedule.initializing = false;
        stopSource(id);
        schedule.enabled = false;
        schedule.recoverAt =
            schedule.attempts < 3 ? at + (1000u << std::min(schedule.attempts, 3u)) : UINT64_MAX;
    }
    portENTER_CRITICAL(&mailboxLock);
    workerMetrics.coalesced += mailbox.publish(sample);
    portEXIT_CRITICAL(&mailboxLock);
    coreWake(CoreWakeWork);
}
void serviceTouch() {
#if FLINT_HAS_TOUCH
    if (touchIrqPending.exchange(false, std::memory_order_relaxed))
        touchWorkerInterrupt();
    touchWorkerMaintain(now());
#endif
}
esp_err_t initializeSource(unsigned id, bool start, bool &complete) {
    complete = false;
    esp_err_t error = ESP_ERR_NOT_SUPPORTED;
    switch (id) {
#if FLINT_HAS_BMP280
    case 1:
        error = start ? bmp.beginAsync(deviceI2cBus(), FLINT_BMP280_ADDRESS) : bmp.maintainInit();
        complete = bmp.ready();
        break;
#endif
#if FLINT_HAS_BME280
    case 2:
        error = start ? bme.beginAsync(deviceI2cBus(), FLINT_BME280_ADDRESS) : bme.maintainInit();
        complete = bme.ready();
        break;
#endif
#if FLINT_HAS_LIGHT
    case 3:
        error = light.begin(deviceI2cBus());
        complete = error == ESP_OK;
        break;
#endif
#if FLINT_HAS_IMU
    case 4:
        error = imu.begin(deviceI2cBus(), FLINT_IMU_ADDRESS);
        complete = error == ESP_OK;
        break;
#endif
    default:
        break;
    }
    return error;
}
void workerRun(void *) {
#if FLINT_HAS_TOUCH
    touchWorkerBegin();
#endif
#if FLINT_HAS_I2C
    deviceI2cWorkerHooks(serviceTouch, [] { return uint64_t(esp_timer_get_time()); });
#endif
    unsigned initStage{};
    uint64_t wakeTarget = UINT64_MAX;
    for (;;) {
        const auto cycleStart = uint64_t(esp_timer_get_time());
        ++workerMetrics.loops;
        const auto at = now();
        uint64_t next = UINT64_MAX;
        if (wakeTarget != UINT64_MAX && at > wakeTarget) {
            const auto late = at - wakeTarget;
            workerMetrics.maxWakeDelayMs = std::max(workerMetrics.maxWakeDelayMs, late);
            workerMetrics.missedDeadlines += late > portTICK_PERIOD_MS;
        }
#if FLINT_HAS_TOUCH
        serviceTouch();
        next = touchWorkerNextWake();
#endif
        // Only system RTC/ADC initialize at boot. Public sensors allocate/probe on demand.
        if (initStage < 2) {
            if (initStage++ == 0) {
#if FLINT_HAS_RTC
                report("rtc", rtc.begin(deviceI2cBus(), FLINT_RTC_ADDRESS));
#endif
            } else {
#if FLINT_HAS_BATTERY
                report("battery",
                    battery.begin(FLINT_BATTERY_ADC, FLINT_BATTERY_DIVIDER_NUMERATOR,
                        FLINT_BATTERY_DIVIDER_DENOMINATOR));
#endif
            }
            vTaskDelay(1);
            continue;
        }
        portENTER_CRITICAL(&mailboxLock);
        if (mailbox.changed) {
            desired = mailbox.desired;
            mailbox.changed = false;
        }
        const bool setRtc = rtcSetPending;
        const auto requestedRtc = rtcDesired;
        const auto rtcWriteTicket = rtcTicket;
        rtcSetPending = false;
        portEXIT_CRITICAL(&mailboxLock);
        (void)setRtc;
        (void)requestedRtc;
        (void)rtcWriteTicket;
#if FLINT_HAS_RTC
        if (setRtc) {
            auto error = rtc.set(requestedRtc);
            std::tm verified{};
            if (error == ESP_OK)
                error = rtc.read(verified);
            if (error == ESP_OK &&
                (flint::hardware::rtcToEpoch(verified) <
                        flint::hardware::rtcToEpoch(requestedRtc) ||
                    flint::hardware::rtcToEpoch(verified) >
                        flint::hardware::rtcToEpoch(requestedRtc) + 1))
                error = ESP_ERR_INVALID_RESPONSE;
            portENTER_CRITICAL(&mailboxLock);
            rtcWriteStatus = error;
            rtcCompleted = rtcWriteTicket;
            rtcWriting = false;
            portEXIT_CRITICAL(&mailboxLock);
            coreWake(CoreWakeWork);
            rtcNext = 0;
        }
        if (at >= rtcNext) {
            std::tm value{};
            const auto error = rtc.read(value);
            portENTER_CRITICAL(&mailboxLock);
            rtcCache = value;
            rtcStatus = error;
            rtcAt = now();
            portEXIT_CRITICAL(&mailboxLock);
            rtcNext = at + 30000;
        }
        next = std::min(next, rtcNext);
#endif
#if FLINT_HAS_BATTERY
        if (at >= batteryNext) {
            int value{};
            serviceTouch();
            const auto adcStart = uint64_t(esp_timer_get_time());
            const auto error = battery.millivolts(value);
            ++workerMetrics.adcReads;
            workerMetrics.adcErrors += error != ESP_OK;
            workerMetrics.maxAdcUs =
                std::max(workerMetrics.maxAdcUs, uint64_t(esp_timer_get_time()) - adcStart);
            portENTER_CRITICAL(&mailboxLock);
            if (error == ESP_OK) {
                batteryMv = value;
                batteryAt = now();
                batteryMeasured = true;
            }
            batteryStatus = error;
            portEXIT_CRITICAL(&mailboxLock);
            batteryNext = at + 30000;
        }
        next = std::min(next, batteryNext);
#endif
        std::array<flint::sensors::Source, flint::sensors::MaxSources> sources{};
        const auto count = catalog(sources);
        for (size_t i = 0; i < count; ++i) {
            const auto &source = sources[i];
            auto &schedule = schedules[source.id];
            flint::sensors::Demand demand{};
            for (const auto &entry : desired)
                if (entry.source == source.id) {
                    demand = entry;
                    break;
                }
            const bool refresh =
                demand.refresh && demand.refresh != schedule.refresh && at < demand.deadline;
            const auto interval =
                demand.intervalMs ? std::max(demand.intervalMs, source.minIntervalMs) : 0;
            const bool needed = interval || refresh;
            if (needed && !schedule.initialized) {
                if (schedule.attempts >= 3 && demand.refresh &&
                    demand.refresh != schedule.failedRefresh) {
                    schedule.attempts = 0;
                    schedule.recoverAt = at;
                }
                if (at < schedule.recoverAt) {
                    next = std::min(next, schedule.recoverAt);
                    continue;
                }
                if (schedule.recoverAt == UINT64_MAX)
                    continue;
                const bool start = !schedule.initializing;
                if (start) {
                    ++schedule.generation;
                    ++schedule.attempts;
                    publish(source.id, source.channels, ESP_OK, {}, now(),
                        flint::sensors::Status::initializing);
                }
                bool complete{};
                const auto error = initializeSource(source.id, start, complete);
                schedule.initializing = error == ESP_OK && !complete;
                schedule.initialized = error == ESP_OK && complete;
                if (error != ESP_OK) {
                    schedule.failedRefresh = demand.refresh;
                    publish(source.id, source.channels, error, {}, now());
                    schedule.refresh = demand.refresh;
                    if (interval)
                        next = std::min(next, schedule.recoverAt);
                } else
                    next = std::min(next, now() + 1);
                continue;
            }
            const auto earliest = std::max(demand.notBefore,
                schedule.measured ? schedule.lastAt + source.minIntervalMs : uint64_t(0));
            const auto periodicDue =
                interval && schedule.measured ? schedule.lastAt + interval : schedule.next;
            const auto due = std::max(earliest, refresh ? uint64_t(0) : periodicDue);
            std::array<flint::sensors::Vector3, 6> values{};
            esp_err_t error = ESP_ERR_NOT_SUPPORTED;
            uint64_t ready = UINT64_MAX;
            switch (source.id) {
#if FLINT_HAS_BMP280
            case 1: {
                ready = bmp.nextWake();
                if (ready == UINT64_MAX && needed && at >= due) {
                    error = bmp.start(at);
                    if (error != ESP_OK) {
                        publish(source.id, source.channels, error, values, at);
                        schedule.refresh = demand.refresh;
                        schedule.failedRefresh = demand.refresh;
                        schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    }
                    ready = bmp.nextWake();
                }
                if (ready != UINT64_MAX && at >= ready) {
                    flint::modules::EnvironmentalSample sample{};
                    error = bmp.read(sample, at);
                    values[0].x = sample.temperatureC;
                    values[1].x = sample.pressurePa;
                    values[2].x = sample.humidityPercent;
                    publish(source.id, source.channels, error, values, now());
                    schedule.refresh = demand.refresh;
                    if (error != ESP_OK)
                        schedule.failedRefresh = demand.refresh;
                    schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    ready = bmp.nextWake();
                }
                break;
            }
#endif
#if FLINT_HAS_BME280
            case 2: {
                ready = bme.nextWake();
                if (ready == UINT64_MAX && needed && at >= due) {
                    error = bme.start(at);
                    if (error != ESP_OK) {
                        publish(source.id, source.channels, error, values, at);
                        schedule.refresh = demand.refresh;
                        schedule.failedRefresh = demand.refresh;
                        schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    }
                    ready = bme.nextWake();
                }
                if (ready != UINT64_MAX && at >= ready) {
                    flint::modules::EnvironmentalSample sample{};
                    error = bme.read(sample, at);
                    values[0].x = sample.temperatureC;
                    values[1].x = sample.pressurePa;
                    values[2].x = sample.humidityPercent;
                    publish(source.id, source.channels, error, values, now());
                    schedule.refresh = demand.refresh;
                    if (error != ESP_OK)
                        schedule.failedRefresh = demand.refresh;
                    schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    ready = bme.nextWake();
                }
                break;
            }
#endif
#if FLINT_HAS_LIGHT
            case 3: {
                ready = light.nextWake();
                if (ready == UINT64_MAX && needed && at >= due) {
                    error = light.start(at);
                    if (error != ESP_OK) {
                        publish(source.id, source.channels, error, values, at);
                        schedule.refresh = demand.refresh;
                        schedule.failedRefresh = demand.refresh;
                        schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    }
                    ready = light.nextWake();
                }
                if (ready != UINT64_MAX && at >= ready) {
                    error = light.read(values[3].x, at);
                    publish(source.id, source.channels, error, values, now());
                    schedule.refresh = demand.refresh;
                    if (error != ESP_OK)
                        schedule.failedRefresh = demand.refresh;
                    schedule.next = at + (interval ? interval : source.defaultIntervalMs);
                    ready = light.nextWake();
                }
                break;
            }
#endif
#if FLINT_HAS_IMU
            case 4: {
                if (needed != schedule.enabled || (needed && schedule.interval != interval)) {
                    error = imu.enable(needed, interval ? interval : 40);
                    schedule.interval = interval;
                    schedule.enabled = needed && error == ESP_OK;
                    if (needed) {
                        schedule.readyAt = at + 40;
                        if (error != ESP_OK) {
                            publish(source.id, source.channels, error, values, at);
                            schedule.readyAt = at + 1000;
                        }
                    }
                }
                ready = needed ? std::max(due, schedule.readyAt) : UINT64_MAX;
                if (needed && at >= ready) {
                    flint::modules::MotionSample sample{};
                    error = imu.read(sample);
                    if (error == ESP_ERR_NOT_FOUND) {
                        publish(source.id, source.channels, ESP_OK, {}, now(),
                            flint::sensors::Status::pending);
                        schedule.readyAt = at + 20;
                        ready = schedule.readyAt;
                        break;
                    }
                    constexpr double gravity = 9.80665 / 16384.0,
                                     radians = 0.017453292519943295 / 64.0;
                    // Fixed device axes follow the configured board's mounted QMI8658 X/Y/Z.
                    // Display rotation never changes these axes.
                    values[4] = {sample.acceleration[0] * gravity, sample.acceleration[1] * gravity,
                        sample.acceleration[2] * gravity};
                    values[5] = {sample.angularVelocity[0] * radians,
                        sample.angularVelocity[1] * radians, sample.angularVelocity[2] * radians};
                    publish(source.id, source.channels, error, values, now());
                    schedule.refresh = demand.refresh;
                    if (error != ESP_OK)
                        schedule.failedRefresh = demand.refresh;
                    schedule.next = at + (interval ? interval : 40);
                    ready = interval ? schedule.next : UINT64_MAX;
                    if (!interval) {
                        report("imu stop", imu.enable(false));
                        schedule.enabled = false;
                    }
                }
                break;
            }
#endif
            default:
                break;
            }
            if (ready != UINT64_MAX)
                next = std::min(next, ready);
            const bool pendingRefresh =
                demand.refresh && demand.refresh != schedule.refresh && at < demand.deadline;
            if ((interval || pendingRefresh) && ready == UINT64_MAX) {
                const auto cooldown =
                    schedule.measured ? schedule.lastAt + source.minIntervalMs : uint64_t(0);
                next = std::min(next,
                    std::max({demand.notBefore, cooldown,
                        pendingRefresh ? uint64_t(0)
                                       : (interval && schedule.measured ? schedule.lastAt + interval
                                                                        : schedule.next)}));
            }
        }
#if FLINT_HAS_TOUCH
        next = std::min(next, touchWorkerNextWake());
#endif
        workerMetrics.maxCycleUs =
            std::max(workerMetrics.maxCycleUs, uint64_t(esp_timer_get_time()) - cycleStart);
        workerMetrics.stackBytes = uxTaskGetStackHighWaterMark(nullptr);
#if FLINT_HAS_I2C
        const auto i2c = deviceI2cMetrics();
        workerMetrics.i2cTransfers = i2c.transfers;
        workerMetrics.i2cErrors = i2c.errors;
        workerMetrics.maxI2cUs = i2c.maxTransferUs;
        workerMetrics.i2cTotalUs = i2c.totalTransferUs;
#endif
        portENTER_CRITICAL(&mailboxLock);
        metricsCache = workerMetrics;
        portEXIT_CRITICAL(&mailboxLock);
        wakeTarget = next;
        TickType_t ticks = portMAX_DELAY;
        if (next != UINT64_MAX)
            ticks = TickType_t(std::max<uint64_t>(1,
                std::min<uint64_t>((next > now() ? next - now() : 0) / portTICK_PERIOD_MS + 1,
                    portMAX_DELAY - 1)));
        uint32_t reason{};
        xTaskNotifyWait(0, UINT32_MAX, &reason, ticks);
        workerMetrics.notifications += reason != 0;
        workerMetrics.timedWakes += reason == 0;
#if FLINT_HAS_TOUCH
        serviceTouch();
#endif
    }
}
#endif // configured worker peripherals
} // namespace
flint::sensors::Backend *deviceSensorBackend() {
#if FLINT_HAS_BMP280 || FLINT_HAS_BME280 || FLINT_HAS_LIGHT || FLINT_HAS_IMU
    return &backend;
#else
    return nullptr;
#endif
}
void deviceHardwareBegin() {
#if FLINT_HAS_POWER_BUTTON
    report("power",
        power.begin(FLINT_POWER_BUTTON_HOLD, FLINT_POWER_BUTTON_BUTTON, FLINT_POWER_BUTTON_BOOT));
    if (power.ready()) {
        buttonOwner = xTaskGetCurrentTaskHandle();
        powerState.seed(power.pressed());
        bootState.seed(power.bootPressed());
        auto error = gpio_install_isr_service(0);
        if (error == ESP_ERR_INVALID_STATE)
            error = ESP_OK;
        if (error == ESP_OK)
            error = gpio_isr_handler_add(gpio_num_t(FLINT_POWER_BUTTON_BUTTON), buttonIrq, nullptr);
        if (error == ESP_OK)
            error = gpio_isr_handler_add(gpio_num_t(FLINT_POWER_BUTTON_BOOT), buttonIrq, nullptr);
        if (error == ESP_OK)
            error = gpio_set_intr_type(gpio_num_t(FLINT_POWER_BUTTON_BUTTON), GPIO_INTR_ANYEDGE);
        if (error == ESP_OK)
            error = gpio_set_intr_type(gpio_num_t(FLINT_POWER_BUTTON_BOOT), GPIO_INTR_ANYEDGE);
        if (error == ESP_OK)
            error = gpio_intr_enable(gpio_num_t(FLINT_POWER_BUTTON_BUTTON));
        if (error == ESP_OK)
            error = gpio_intr_enable(gpio_num_t(FLINT_POWER_BUTTON_BOOT));
        report("button IRQ", error);
    }
#endif
#if FLINT_HAS_BUZZER
    report("buzzer", buzzer.begin(FLINT_BUZZER_OUTPUT));
#endif
#if FLINT_HAS_BACKLIGHT
    report("backlight", backlight.begin(FLINT_BACKLIGHT_OUTPUT));
#endif
#if FLINT_HAS_I2C || FLINT_HAS_BATTERY
    {
        if (xTaskCreate(workerRun, "hardware", 4096, nullptr, 6, &worker) != pdPASS) {
            worker = nullptr;
            ESP_LOGE("hardware", "worker allocation failed");
        }
    }
#endif
}
void deviceHardwareWakeFromIsr() {
#if FLINT_HAS_I2C || FLINT_HAS_BATTERY
    touchIrqPending.store(true, std::memory_order_relaxed);
    BaseType_t higherPriorityWoken = pdFALSE;
    if (worker)
        xTaskNotifyFromISR(worker, 2, eSetBits, &higherPriorityWoken);
    if (higherPriorityWoken)
        portYIELD_FROM_ISR();
#endif
}
void deviceHardwareDiagnostics(flint::Values &values) {
#if FLINT_HAS_I2C || FLINT_HAS_BATTERY
    values["hardware_worker_ready"] = worker ? "1" : "0";
    portENTER_CRITICAL(&mailboxLock);
    const auto metrics = metricsCache;
    portEXIT_CRITICAL(&mailboxLock);
    values["hardware_worker_loops"] = std::to_string(metrics.loops);
    values["hardware_worker_notifications"] = std::to_string(metrics.notifications);
    values["hardware_worker_timed_wakes"] = std::to_string(metrics.timedWakes);
    values["hardware_worker_missed_deadlines"] = std::to_string(metrics.missedDeadlines);
    values["hardware_worker_max_wake_delay_ms"] = std::to_string(metrics.maxWakeDelayMs);
    values["hardware_worker_max_cycle_us"] = std::to_string(metrics.maxCycleUs);
    values["hardware_worker_stack_watermark_bytes"] = std::to_string(metrics.stackBytes);
    values["hardware_sensor_max_gap_ms"] = std::to_string(metrics.maxGapMs);
    values["hardware_samples_coalesced"] = std::to_string(metrics.coalesced);
    values["hardware_i2c_transfers"] = std::to_string(metrics.i2cTransfers);
    values["hardware_i2c_errors"] = std::to_string(metrics.i2cErrors);
    values["hardware_i2c_max_transfer_us"] = std::to_string(metrics.maxI2cUs);
    values["hardware_i2c_total_transfer_us"] = std::to_string(metrics.i2cTotalUs);
    values["hardware_adc_reads"] = std::to_string(metrics.adcReads);
    values["hardware_adc_errors"] = std::to_string(metrics.adcErrors);
    values["hardware_adc_max_conversion_us"] = std::to_string(metrics.maxAdcUs);
#else
    values["hardware_worker_ready"] = "0";
#endif
    // Public diagnostics deliberately contain no permission-controlled sensor readings.
}
flint::Error devicePowerStatus(std::string_view key, flint::AppReply &reply) {
#if FLINT_HAS_BATTERY
    portENTER_CRITICAL(&mailboxLock);
    const auto error = batteryStatus;
    const auto mv = batteryMv;
    const auto measuredAt = batteryAt;
    const auto measured = batteryMeasured;
    portEXIT_CRITICAL(&mailboxLock);
    const auto age = now() - measuredAt;
    if (key == "status")
        return reply.assign(error != ESP_OK ? "failed" : age > 90000 ? "stale" : "valid");
    if (key == "age_ms") {
        if (!measured)
            return flint::Error::source_unavailable;
        reply.number = age;
    } else if (key == "measured_at_ms") {
        if (!measured)
            return flint::Error::source_unavailable;
        reply.number = measuredAt;
    } else if (key == "voltage_mv") {
        if (error != ESP_OK || age > 90000)
            return flint::Error::source_unavailable;
        reply.number = mv;
    } else
        return flint::Error::not_supported;
    reply.numeric = true;
    return flint::Error::ok;
#else
    (void)key;
    (void)reply;
    return flint::Error::not_supported;
#endif
}
esp_err_t deviceBacklight(unsigned percent) {
#if FLINT_HAS_BACKLIGHT
    return backlight.set(percent);
#else
    (void)percent;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t deviceTone(unsigned frequency, unsigned durationMs) {
#if FLINT_HAS_BUZZER
    if (muted && durationMs)
        return ESP_ERR_INVALID_STATE;
    return buzzer.tone(frequency, durationMs, now());
#else
    (void)frequency;
    (void)durationMs;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
void deviceButtonsInterrupt() {
#if FLINT_HAS_POWER_BUTTON
    powerState.edge(now());
    bootState.edge(now());
#endif
}
void deviceHardwareMaintain() {
#if FLINT_HAS_POWER_BUTTON
    const auto at = now(), generation = coreRuntimeGeneration();
    powerState.maintain(power.pressed(), 1, generation, at, buttonPost, nullptr);
    bootState.maintain(power.bootPressed(), 2, generation, at, buttonPost, nullptr);
#endif
#if FLINT_HAS_BUZZER
    report("buzzer stop", buzzer.maintain(now()));
#endif
}
uint64_t deviceHardwareNextWake() {
    uint64_t next = UINT64_MAX;
#if FLINT_HAS_BUZZER
    next = std::min(next, buzzer.nextWake());
#endif
#if FLINT_HAS_POWER_BUTTON
    next = std::min({next, powerState.nextWake(), bootState.nextWake()});
#endif
    return next;
}
esp_err_t deviceReleasePower() {
#if FLINT_HAS_POWER_BUTTON
    return power.releasePower();
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t deviceSetMuted(bool value) {
    muted = value;
#if FLINT_HAS_BUZZER
    return value ? buzzer.tone(2000, 0, now()) : ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
flint::Error deviceRtcReadEpoch(uint64_t &epoch) {
#if FLINT_HAS_RTC
    portENTER_CRITICAL(&mailboxLock);
    const auto value = rtcCache;
    const auto error = rtcStatus;
    const auto at = rtcAt;
    portEXIT_CRITICAL(&mailboxLock);
    if (error != ESP_OK)
        return flint::Error::source_unavailable;
    epoch = flint::hardware::rtcToEpoch(value) + (now() - at) / 1000;
    return flint::Error::ok;
#else
    (void)epoch;
    return flint::Error::not_supported;
#endif
}
flint::Error deviceRtcRequest(uint64_t epoch, uint64_t &token) {
#if FLINT_HAS_RTC
    std::tm utc{};
    if (!flint::hardware::rtcFromEpoch(epoch, utc))
        return flint::Error::invalid_argument;
    if (!worker)
        return flint::Error::source_unavailable;
    portENTER_CRITICAL(&mailboxLock);
    if (rtcWriting) {
        portEXIT_CRITICAL(&mailboxLock);
        return flint::Error::invalid_state;
    }
    token = ++rtcTicket;
    rtcDesired = utc;
    rtcSetPending = rtcWriting = true;
    portEXIT_CRITICAL(&mailboxLock);
    wakeWorker();
    return flint::Error::ok;
#else
    (void)epoch;
    (void)token;
    return flint::Error::not_supported;
#endif
}
flint::Error deviceRtcResult(uint64_t token, bool &complete) {
    complete = false;
#if FLINT_HAS_RTC
    portENTER_CRITICAL(&mailboxLock);
    const auto ticket = rtcTicket, completed = rtcCompleted;
    const auto error = rtcWriteStatus;
    portEXIT_CRITICAL(&mailboxLock);
    if (!token || token != ticket)
        return flint::Error::stale_handle;
    complete = token == completed;
    return !complete || error == ESP_OK ? flint::Error::ok : flint::Error::io_error;
#else
    (void)token;
    return flint::Error::not_supported;
#endif
}
