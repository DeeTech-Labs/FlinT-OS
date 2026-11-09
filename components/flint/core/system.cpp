#include "ble/ble_link.h"
#include "core/bridge.h"
#include "display/display.h"
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <memory>
#include <unistd.h>
static std::unique_ptr<flint::Core> core;
static flint::FrameParser usbParser;
static TaskHandle_t coreTask{};
static uint64_t maxTickUs{}, maxFrameUs{};
static flint::Session usbSession{true, false, 1};

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
flint::Frame coreRequest(const flint::Frame &frame, flint::Session &session) {
    return core->handle(frame, session);
}
void coreDisconnect(flint::Session &session) {
    if (core)
        core->disconnect(session);
}
uint64_t coreRuntimeGeneration() {
    return core && xTaskGetCurrentTaskHandle() == coreTask ? core->runtime().generation() : 0;
}
flint::Error corePostInput(flint::InputEvent event) {
    if (!core || xTaskGetCurrentTaskHandle() != coreTask)
        return flint::Error::invalid_state;
    return core->postInput(event);
}
void appSetup(bool trustReady) {
    coreTask = xTaskGetCurrentTaskHandle();
    displayBegin();
    bool factory = mount("factory", "/factory", true);
    bool storage = mount("storage", "/storage");
    usb_serial_jtag_driver_config_t usb{2048, 2048};
    usb_serial_jtag_driver_install(&usb);
    flint::Platform platform;
    const auto *description = esp_app_get_description();
    const char *hex = "0123456789abcdef";
    for (auto byte : description->app_elf_sha256) {
        platform.build_id += hex[byte >> 4];
        platform.build_id += hex[byte & 15];
    }
    platform.capabilities |= flint::CapUsb;
    if (trustReady) {
        platform.capabilities |= flint::CapBle;
    }
    const auto internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const auto external = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    platform.memory = {
        uint32_t(internal > 32768 ? std::min<size_t>(internal - 32768, 48 * 1024) : 0),
        uint32_t(external), 4096};
    platform.diagnostics = [] {
        flint::Values values;
        values["internal_free_bytes"] = std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["internal_largest_bytes"] = std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["internal_minimum_bytes"] = std::to_string(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        values["psram_free_bytes"] = std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        values["main_stack_watermark_bytes"] = std::to_string(uxTaskGetStackHighWaterMark(nullptr));
        values["max_tick_us"] = std::to_string(maxTickUs);
        values["max_frame_us"] = std::to_string(maxFrameUs);
        return values;
    };
    // The selected Spark profile has no battery sensor. Never synthesize telemetry.
    platform.power = [](std::string_view, flint::AppReply &) {
        return flint::Error::not_supported;
    };
    platform.monotonic = [] { return uint64_t(esp_timer_get_time() / 1000); };
    platform.free_storage = [] {
        size_t total = 0, used = 0;
        return esp_littlefs_info("storage", &total, &used) == ESP_OK && used <= total
            ? uint64_t(total - used)
            : 0;
    };
    platform.pairing = [](std::string_view action) {
        char buffer[768];
        std::string text(action);
        if (!bleLinkPairingCommand(text.c_str(), buffer, sizeof(buffer)))
            return flint::Bytes{};
        return flint::Bytes(buffer, buffer + strlen(buffer));
    };
    core = std::make_unique<flint::Core>("/storage/packages", "/factory", platform);
    if (!storage || !factory || core->begin() != flint::Error::ok)
        displayRecovery("Storage or factory packages unavailable");
    if (trustReady) {
        bleLinkBegin();
    }
}
void appLoop() {
    uint8_t bytes[256];
    const auto at = core->platform().monotonic();
    const auto deadline = core->nextWake();
    // USB traffic wakes the read immediately. BLE maintenance retains its existing 20 ms bound;
    // application timers can request an earlier wake without a permanent 2 ms polling loop.
    const auto waitMs = deadline <= at ? 0 : std::min<uint64_t>(20, deadline - at);
    const auto waitTicks = pdMS_TO_TICKS(waitMs);
    auto count = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), waitTicks);
    if (count > 0)
        usbParser.feed({bytes, size_t(count)}, [](flint::Frame frame) {
            auto bytes = flint::encodeFrame(coreRequest(frame, usbSession));
            size_t offset = 0;
            while (offset < bytes.size()) {
                auto sent = usb_serial_jtag_write_bytes(
                    bytes.data() + offset, bytes.size() - offset, pdMS_TO_TICKS(100));
                if (sent <= 0)
                    break;
                offset += sent;
            }
        });
    static bool attached = false;
    bool connected = usb_serial_jtag_is_connected();
    if (attached && !connected) {
        coreDisconnect(usbSession);
        usbParser.reset();
    }
    attached = connected;
    bleLinkMaintain();
    const auto tickStart = esp_timer_get_time();
    const bool dirty = core->tick();
    maxTickUs = std::max(maxTickUs, uint64_t(esp_timer_get_time() - tickStart));
    if (dirty) {
        const auto frameStart = esp_timer_get_time();
        FlintRender::Scene scene;
        if (core->scene(scene) == flint::Error::ok)
            displayScene(*core, scene);
        maxFrameUs = std::max(maxFrameUs, uint64_t(esp_timer_get_time() - frameStart));
    }
    // A zero-time read or sustained USB traffic must still let lower-priority tasks run.
    if (!waitTicks || count > 0)
        vTaskDelay(1);
}
