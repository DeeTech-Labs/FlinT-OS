#include "ble/ble_link.h"
#include "core/bridge.h"
#include "display/display.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input/touch.h"
#include "hardware/peripherals.h"
#include "sdkconfig.h"
#include "usb/usb_link.h"
#include <cstring>
#include <memory>
#include <unistd.h>
#if CONFIG_BT_NIMBLE_ENABLED
static constexpr bool BleSupported = true;
#else
static constexpr bool BleSupported = false;
#endif
static std::unique_ptr<flint::Core> core;
static TaskHandle_t coreTask{};
static uint64_t maxTickUs{}, maxFrameUs{};
struct Counters {
    uint64_t loops{}, usbWakes{}, timedWakes{}, bleWakes{}, txWakes{}, sessionWakes{}, requests{},
        requestErrors{}, maxRequestUs{}, maxSceneUs{};
    size_t internalFree{}, internalLargest{}, psramFree{}, psramLargest{};
};
static Counters counters;
static bool bootSnapshotReady{}, expectBleInitialization{};
static void captureMemoryBaseline() {
    if (bootSnapshotReady)
        return;
    const auto *state = bleLinkState();
    if (expectBleInitialization &&
        (!strcmp(state, "starting") || !strcmp(state, "host_starting") ||
            !strcmp(state, "synced") || !strcmp(state, "stopping")))
        return;
    counters.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    counters.internalLargest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    counters.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    counters.psramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bootSnapshotReady = true;
}
static uint64_t nextSessionId = 1;
uint64_t coreNewSession() {
    return ++nextSessionId;
}

static bool mount(const char *name, const char *path, bool readOnly = false) {
    esp_vfs_littlefs_conf_t config{};
    config.partition_label = name;
    config.base_path = path;
    config.read_only = readOnly;
    // Format only an entirely erased writable partition. Corruption must reach recovery.
    const auto *partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, name);
    bool erased = partition && !readOnly;
    std::array<uint8_t, 256> buffer{};
    if (erased)
        for (uint32_t offset = 0; offset < partition->size; offset += buffer.size()) {
            auto count = std::min<uint32_t>(buffer.size(), partition->size - offset);
            if (esp_partition_read(partition, offset, buffer.data(), count) != ESP_OK) {
                erased = false;
                break;
            }
            for (size_t i = 0; i < count; ++i)
                if (buffer[i] != 255) {
                    erased = false;
                    break;
                }
            if (!erased)
                break;
        }
    config.format_if_mount_failed = erased;
    return esp_vfs_littlefs_register(&config) == ESP_OK;
}
std::optional<flint::Frame> coreRequest(const flint::Frame &frame, flint::Session &session) {
    const auto start = esp_timer_get_time();
    auto reply = core->process(frame, session);
    ++counters.requests;
    if (reply && (reply->payload.size() < 2 || flint::u16(reply->payload.data())))
        ++counters.requestErrors;
    counters.maxRequestUs = std::max(counters.maxRequestUs, uint64_t(esp_timer_get_time() - start));
    return reply;
}
bool coreTakeReply(const flint::Session &session, flint::Frame &reply) {
    if (!core || !core->takeReply(session, reply))
        return false;
    if (reply.payload.size() < 2 || flint::u16(reply.payload.data()))
        ++counters.requestErrors;
    return true;
}
bool coreReplyReady(const flint::Session &session) {
    return core && core->replyReady(session);
}
void coreDisconnect(flint::Session &session) {
    if (core)
        core->disconnect(session);
}
uint64_t coreRuntimeGeneration() {
    return xTaskGetCurrentTaskHandle() == coreTask && core ? core->runtime().generation() : 0;
}
uint32_t coreApplicationReserve() {
    if (xTaskGetCurrentTaskHandle() != coreTask || !core ||
        core->runtime().state() == flint::State::empty)
        return 0;
    const auto &runtime = core->runtime();
    const auto used = runtime.logicUsed() + runtime.nativeHeapEstimate();
    const auto declared = runtime.manifest().budget.internal_bytes;
    return used < declared ? uint32_t(declared - used) : 0;
}
flint::Error corePostInput(flint::InputEvent event) {
    if (xTaskGetCurrentTaskHandle() != coreTask || !core)
        return flint::Error::invalid_state;
    return core->postInput(event);
}
void appSetup(bool trustReady) {
    expectBleInitialization = trustReady && BleSupported;
    coreTask = xTaskGetCurrentTaskHandle();
    deviceHardwareBegin();
    displayBegin();
    bool factory = mount("factory", "/factory", true);
    bool storage = mount("storage", "/storage");
    const bool usbReady = usbLinkBegin();
    flint::Platform platform;
    platform.require_factory = true;
    platform.power_on = esp_reset_reason() == ESP_RST_POWERON;
    platform.system_ui = true;
    platform.pairing_prompt = bleLinkPairingPrompt;
    platform.present = [](const FlintRender::Scene &scene) {
        const auto start = esp_timer_get_time();
        displayScene(*core, scene);
        maxFrameUs = std::max(maxFrameUs, uint64_t(esp_timer_get_time() - start));
    };
    // esp_littlefs rejects rename/unlink while a VFS file descriptor is open.
    platform.rename_open_files = false;
    const auto *description = esp_app_get_description();
    const char *hex = "0123456789abcdef";
    for (auto byte : description->app_elf_sha256) {
        platform.build_id += hex[byte >> 4];
        platform.build_id += hex[byte & 15];
    }
    if (usbReady)
        platform.capabilities |= flint::CapUsb;
    if (trustReady && BleSupported) {
        platform.capabilities |= flint::CapBle | flint::CapBlePolicy;
        platform.ble_check = [](bool enabled) {
            return bleLinkCheckEnabled(enabled, coreApplicationReserve());
        };
        platform.ble_apply = bleLinkSetEnabled;
    }
    // Stable profile limits determine package admissibility, never the current free heap.
    platform.memory = {
        48 * 1024, uint32_t(heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)), 4096};
    platform.available_memory = [] {
        const auto budget = [](uint32_t caps, size_t reserve) {
            const auto free = heap_caps_get_free_size(caps);
            const auto largest = heap_caps_get_largest_free_block(caps);
            return uint32_t(std::min(free > reserve ? free - reserve : 0, largest));
        };
        // During asynchronous BLE initialization retain additional startup headroom.
        const auto startupReserve = !bootSnapshotReady && expectBleInitialization ? 32768 : 0;
        auto internal = budget(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
            CONFIG_FLINT_INTERNAL_MEMORY_RESERVE + startupReserve);
        return flint::MemoryBudget{std::min<uint32_t>(internal, 48 * 1024),
            budget(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, CONFIG_FLINT_PSRAM_MEMORY_RESERVE), 4096};
    };
    platform.diagnostics = [] {
        flint::Values values;
        values["internal_reserve_bytes"] = std::to_string(CONFIG_FLINT_INTERNAL_MEMORY_RESERVE);
        values["psram_reserve_bytes"] = std::to_string(CONFIG_FLINT_PSRAM_MEMORY_RESERVE);
        values["internal_free_bytes"] =
            std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["internal_largest_bytes"] =
            std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["internal_minimum_bytes"] =
            std::to_string(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["psram_free_bytes"] =
            std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        values["psram_largest_bytes"] =
            std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        values["psram_minimum_bytes"] =
            std::to_string(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        values["main_stack_watermark_bytes"] = std::to_string(uxTaskGetStackHighWaterMark(nullptr));
        values["max_tick_us"] = std::to_string(maxTickUs);
        values["max_frame_us"] = std::to_string(maxFrameUs);
        values["max_scene_us"] = std::to_string(counters.maxSceneUs);
        values["max_request_us"] = std::to_string(counters.maxRequestUs);
        values["requests"] = std::to_string(counters.requests);
        values["request_errors"] = std::to_string(counters.requestErrors);
        values["loop_count"] = std::to_string(counters.loops);
        values["usb_rx_wakes"] = std::to_string(counters.usbWakes);
        values["usb_read_empty_wakes"] = std::to_string(counters.timedWakes);
        values["ble_wakes"] = std::to_string(counters.bleWakes);
        values["usb_tx_wakes"] = std::to_string(counters.txWakes);
        values["usb_session_wakes"] = std::to_string(counters.sessionWakes);
        usbLinkDiagnostics(values);
        values["boot_internal_free_bytes"] = std::to_string(counters.internalFree);
        values["boot_snapshot_ready"] = bootSnapshotReady ? "1" : "0";
        values["ble_state"] = bleLinkState();
        values["boot_internal_largest_bytes"] = std::to_string(counters.internalLargest);
        values["boot_psram_free_bytes"] = std::to_string(counters.psramFree);
        values["boot_psram_largest_bytes"] = std::to_string(counters.psramLargest);
        displayDiagnostics(values);
        deviceHardwareDiagnostics(values);
        bleLinkDiagnostics(values);
        values["touch_ready"] = touchReady() ? "1" : "0";
        return values;
    };
    platform.power = devicePowerStatus;
    platform.monotonic = [] { return uint64_t(esp_timer_get_time() / 1000); };
    platform.microseconds = [] { return uint64_t(esp_timer_get_time()); };
    platform.free_storage = [] {
        size_t total = 0, used = 0;
        return esp_littlefs_info("storage", &total, &used) == ESP_OK && used <= total
            ? uint64_t(total - used)
            : 0;
    };
    platform.pairing = [](std::string_view action) {
        if (action.find('\0') != std::string_view::npos)
            return flint::Bytes{};
        char buffer[768];
        std::string text(action);
        if (!bleLinkPairingCommand(text.c_str(), buffer, sizeof(buffer)))
            return flint::Bytes{};
        return flint::Bytes(buffer, buffer + strlen(buffer));
    };
    core = std::make_unique<flint::Core>("/storage/packages", "/factory", platform);
    if (!storage || !factory || core->begin() != flint::Error::ok)
        displayRecovery("Storage or factory packages unavailable");
    touchBegin();
    bleLinkMaintain();
    captureMemoryBaseline();
    coreWake(CoreWakeWork);
}
void coreWake(uint32_t reason) {
    if (coreTask)
        xTaskNotify(coreTask, reason, eSetBits);
}
bool coreTakeCompanionEvent(const flint::Session &session, flint::Frame &event) {
    return core && core->takeCompanionEvent(session, event);
}
uint64_t coreCompanionNextWake(const flint::Session &session) {
    return core ? core->companionNextWake(session) : UINT64_MAX;
}
void appLoop() {
    const auto at = core->platform().monotonic();
    const auto deadline = std::min({core->nextWake(), bleLinkNextWake(), usbLinkNextWake(),
        touchNextWake(), deviceHardwareNextWake()});
    TickType_t waitTicks = portMAX_DELAY;
    if (deadline != UINT64_MAX) {
        const auto delay = deadline <= at ? 0 : deadline - at;
        waitTicks = TickType_t(std::min<uint64_t>(
            (delay + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS, portMAX_DELAY - 1));
    }
    uint32_t reason = 0;
    xTaskNotifyWait(0, UINT32_MAX, &reason, waitTicks);
    ++counters.loops;
    if (reason & CoreWakeUsbRx)
        ++counters.usbWakes;
    if (reason & CoreWakeUsbTx)
        ++counters.txWakes;
    if (reason & CoreWakeUsbSession)
        ++counters.sessionWakes;
    if (reason & CoreWakeBle)
        ++counters.bleWakes;
    if (!reason)
        ++counters.timedWakes;
    usbLinkMaintain();
    bleLinkMaintain();
    if (reason & CoreWakeTouch)
        touchInterrupt();
    touchMaintain();
    deviceHardwareMaintain();
    captureMemoryBaseline();
    const auto tickStart = esp_timer_get_time();
    const bool dirty = core->tick();
    maxTickUs = std::max(maxTickUs, uint64_t(esp_timer_get_time() - tickStart));
    if (dirty) {
        const auto frameStart = esp_timer_get_time();
        FlintRender::Scene scene;
        const auto sceneStart = esp_timer_get_time();
        const auto error = core->scene(scene);
        counters.maxSceneUs =
            std::max(counters.maxSceneUs, uint64_t(esp_timer_get_time() - sceneStart));
        if (error == flint::Error::ok)
            displayScene(*core, scene);
        maxFrameUs = std::max(maxFrameUs, uint64_t(esp_timer_get_time() - frameStart));
    }
    // Sustained work must still let lower-priority tasks run.
    if (reason || !waitTicks)
        vTaskDelay(1);
}
