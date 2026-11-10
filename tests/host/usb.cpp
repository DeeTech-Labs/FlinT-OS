#include "core/bridge.h"
#include "driver/usb_serial_jtag_select.h"
#include "esp_freertos_hooks.h"
#include "freertos/task.h"
#include "usb/usb_link.h"
#include <cstdio>
#include <deque>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
int64_t usbTestClock = 0;
namespace {
bool connected = true, backpressure = false;
unsigned requests{}, disconnects{}, installs{}, uninstalls{}, wakes{};
uint64_t sessionId{}, lastSession{};
size_t rxCapacity{};
std::deque<uint8_t> incoming;
flint::Bytes outgoing;
void (*tickHook)(){};
usj_select_notif_callback_t callback{};
bool eventPending{};
bool asyncRequested{};
uint64_t asyncSession{};
std::optional<flint::Frame> deferred;
void feed(const flint::Frame &frame) {
    const auto bytes = flint::encodeFrame(frame);
    CHECK(incoming.size() + bytes.size() <= rxCapacity);
    incoming.insert(incoming.end(), bytes.begin(), bytes.end());
    int ignored = 0;
    if (callback)
        callback(USJ_SELECT_READ_NOTIF, &ignored);
}
} // namespace
TaskHandle_t xTaskGetCurrentTaskHandle() {
    return reinterpret_cast<void *>(1);
}
BaseType_t xTaskNotifyFromISR(TaskHandle_t, uint32_t bits, int, BaseType_t *) {
    wakes |= bits;
    return pdTRUE;
}
esp_err_t esp_register_freertos_tick_hook_for_cpu(void (*hook)(), unsigned cpu) {
    CHECK(cpu == 0);
    tickHook = hook;
    return ESP_OK;
}
bool usb_serial_jtag_is_connected() {
    return connected;
}
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t *config) {
    CHECK(config->rx_buffer_size >= flint::MaxPayload + 40 &&
        config->rx_buffer_size < flint::MaxPayload + 104);
    CHECK(config->tx_buffer_size == 2048);
    rxCapacity = config->rx_buffer_size;
    ++installs;
    return ESP_OK;
}
esp_err_t usb_serial_jtag_driver_uninstall() {
    ++uninstalls;
    incoming.clear();
    return ESP_OK;
}
void usb_serial_jtag_set_select_notif_callback(usj_select_notif_callback_t value) {
    callback = value;
}
bool usb_serial_jtag_read_ready() {
    return !incoming.empty();
}
int usb_serial_jtag_read_bytes(void *buffer, uint32_t size, uint32_t wait) {
    CHECK(wait == 0 && size <= 256);
    auto *bytes = static_cast<uint8_t *>(buffer);
    unsigned count = 0;
    while (count < size && !incoming.empty()) {
        bytes[count++] = incoming.front();
        incoming.pop_front();
    }
    return int(count);
}
int usb_serial_jtag_write_bytes(const void *buffer, size_t size, uint32_t wait) {
    CHECK(wait == 0 && size <= 256);
    if (backpressure)
        return 0;
    const auto *bytes = static_cast<const uint8_t *>(buffer);
    outgoing.insert(outgoing.end(), bytes, bytes + size);
    return int(size);
}
void coreWake(uint32_t reason) {
    wakes |= reason;
}
uint64_t coreNewSession() {
    return ++sessionId;
}
void coreDisconnect(flint::Session &session) {
    ++disconnects;
    if (session.id == asyncSession)
        deferred.reset();
}
std::optional<flint::Frame> coreRequest(const flint::Frame &frame, flint::Session &session) {
    ++requests;
    lastSession = session.id;
    session.compatible = true;
    if (frame.method == flint::Method::install_commit) {
        asyncRequested = true;
        asyncSession = session.id;
        return std::nullopt;
    }
    return flint::Frame{frame.method, frame.id, true, flint::Bytes(800, uint8_t(frame.id))};
}
bool coreTakeReply(const flint::Session &session, flint::Frame &reply) {
    if (!deferred || session.id != asyncSession)
        return false;
    reply = std::move(*deferred);
    deferred.reset();
    return true;
}
bool coreReplyReady(const flint::Session &session) {
    return deferred && session.id == asyncSession;
}
bool coreTakeCompanionEvent(const flint::Session &, flint::Frame &frame) {
    if (!eventPending)
        return false;
    frame = {flint::Method::companion_state, 0, false, {1, 2}};
    eventPending = false;
    return true;
}
uint64_t coreCompanionNextWake(const flint::Session &) {
    return eventPending ? 0 : UINT64_MAX;
}
int main() {
    try {
        CHECK(usbLinkBegin());
        wakes = 0;
        for (int i = 0; i < 100; ++i)
            tickHook();
        CHECK(wakes == 0); // Inactive link does not wake its owner.
        backpressure = true;
        feed({flint::Method::info, 1, false, {}});
        feed({flint::Method::info, 2, false, {}});
        usbLinkMaintain();
        CHECK(requests == 1 && outgoing.empty());
        eventPending = true;
        usbLinkMaintain();
        CHECK(requests == 1 && eventPending); // Backpressure preserves the next request/event.
        backpressure = false;
        int ignored = 0;
        callback(USJ_SELECT_WRITE_NOTIF, &ignored);
        for (int i = 0; i < 15; ++i)
            usbLinkMaintain();
        CHECK(requests == 2 && !eventPending);
        std::vector<flint::Frame> frames;
        flint::FrameParser parser;
        parser.feed(outgoing, [&](flint::Frame frame) { frames.push_back(std::move(frame)); });
        CHECK(frames.size() == 3 && frames[0].id == 1 && frames[1].id == 2);
        CHECK(flint::isCompanionEvent(frames[2]));
        // Disconnect during a partial response discards both adapter/driver buffers.
        outgoing.clear();
        feed({flint::Method::info, 3, false, {}});
        usbLinkMaintain();
        CHECK(outgoing.size() == 256);
        const auto previousSession = lastSession;
        // Delayed tick callbacks during Flash activity must not look like unplug.
        connected = false;
        for (unsigned i = 0; i < 100; ++i)
            tickHook(); // No elapsed microseconds.
        usbLinkMaintain();
        CHECK(disconnects == 0);
        connected = true;
        tickHook();
        CHECK(disconnects == 0);
        connected = false;
        tickHook();
        usbTestClock += 19999;
        tickHook();
        usbLinkMaintain();
        CHECK(disconnects == 0);
        ++usbTestClock;
        tickHook();
        usbLinkMaintain();
        CHECK(disconnects == 1 && uninstalls == 1);
        outgoing.clear();
        connected = true;
        tickHook();
        usbLinkMaintain();
        CHECK(installs == 2);
        feed({flint::Method::info, 4, false, {}});
        for (int i = 0; i < 6; ++i)
            usbLinkMaintain();
        CHECK(lastSession != previousSession);
        frames.clear();
        parser.reset();
        parser.feed(outgoing, [&](flint::Frame frame) { frames.push_back(std::move(frame)); });
        CHECK(frames.size() == 1 && frames[0].id == 4);
        outgoing.clear();
        feed({flint::Method::install_commit, 9, false, {}});
        usbLinkMaintain();
        CHECK(asyncRequested && outgoing.empty());
        deferred = flint::Frame{flint::Method::install_commit, 9, true, flint::Bytes(800, 9)};
        CHECK(usbLinkNextWake() == 0);
        for (int i = 0; i < 6; ++i)
            usbLinkMaintain();
        frames.clear();
        parser.reset();
        parser.feed(outgoing, [&](flint::Frame frame) { frames.push_back(std::move(frame)); });
        CHECK(frames.size() == 1 && frames[0].id == 9 && frames[0].reply);
        // The owner cannot drain while painting. A maximum normal frame and
        // empty deferred commit must fit the driver together without dropping bytes.
        outgoing.clear();
        const auto previousRequests = requests;
        feed({flint::Method::install_commit, 10, false, {}});
        feed({flint::Method::info, 11, false, flint::Bytes(flint::MaxPayload, 7)});
        for (unsigned i = 0; i < 30; ++i)
            usbLinkMaintain();
        CHECK(requests == previousRequests + 2 && incoming.empty());
        frames.clear();
        parser.reset();
        parser.feed(outgoing, [&](flint::Frame frame) { frames.push_back(std::move(frame)); });
        CHECK(frames.size() == 1 && frames[0].id == 11);
        puts("USB bounded RX/TX, whole frames, backpressure, wakeups and disconnect: passed");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
