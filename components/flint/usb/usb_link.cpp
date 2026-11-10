#include "usb_link.h"
#include "core/bridge.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_select.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// esp_freertos_hooks.h includes portmacro.h and requires FreeRTOS.h first on IDF 6.1 SMP.
#include "esp_freertos_hooks.h"

namespace {
TaskHandle_t owner{};
bool ready{};
bool driverReady{};
portMUX_TYPE connectionLock = portMUX_INITIALIZER_UNLOCKED;
bool connected{};
uint32_t connectionGeneration{}, sofGlitches{};
bool disconnectPending{};
int64_t disconnectSince{};
struct ConnectionState {
    bool connected;
    uint32_t generation;
    uint32_t glitches;
};
ConnectionState connectionState() {
    portENTER_CRITICAL(&connectionLock);
    const ConnectionState result{connected, connectionGeneration, sofGlitches};
    portEXIT_CRITICAL(&connectionLock);
    return result;
}
uint32_t observedGeneration{};
flint::Session session{};
flint::FrameParser parser;
flint::Bytes tx;
size_t txOffset{}, rxOffset{}, rxSize{};
std::array<uint8_t, 256> rx{};
uint64_t rxBytes{}, txBytes{}, disconnects{}, txErrors{};
esp_err_t initError{};

void IRAM_ATTR notifyFromIsr(uint32_t reason, int *woken) {
    BaseType_t taskWoken = pdFALSE;
    xTaskNotifyFromISR(owner, reason, eSetBits, &taskWoken);
    if (taskWoken && woken)
        *woken = pdTRUE;
}
void IRAM_ATTR notification(usj_select_notif_t event, int *woken) {
    notifyFromIsr(event == USJ_SELECT_READ_NOTIF ? CoreWakeUsbRx : CoreWakeUsbTx, woken);
}
// The IDF SOF monitor can briefly report absence after delayed Flash/tick work.
// Require 20 ms of continuous absence on the IRAM microsecond clock, rather than
// counting delayed tick callbacks. Logical session_close/hello still act immediately.
void IRAM_ATTR connectionTick() {
    const bool raw = usb_serial_jtag_is_connected();
    portENTER_CRITICAL_ISR(&connectionLock);
    bool current = connected;
    if (raw) {
        if (disconnectPending && connected)
            ++sofGlitches;
        disconnectPending = false;
        current = true;
    } else if (connected) {
        const auto at = esp_timer_get_time();
        if (!disconnectPending) {
            disconnectPending = true;
            disconnectSince = at;
        }
        if (at - disconnectSince >= 20000)
            current = false;
    }
    const bool changed = current != connected;
    if (changed) {
        connected = current;
        ++connectionGeneration;
    }
    portEXIT_CRITICAL_ISR(&connectionLock);
    if (changed) {
        BaseType_t woken = pdFALSE;
        xTaskNotifyFromISR(owner, CoreWakeUsbSession, eSetBits, &woken);
        if (woken)
            portYIELD_FROM_ISR();
    }
}
bool installDriver() {
    // IDF drops RX bytes when its ring is full. Hold one maximum frame plus the
    // empty deferred commit/close request while the owner is painting/writing Flash.
    constexpr size_t RxCapacity = ((flint::MaxPayload + 40 + 63) / 64) * 64;
    usb_serial_jtag_driver_config_t config{2048, RxCapacity};
    initError = usb_serial_jtag_driver_install(&config);
    driverReady = initError == ESP_OK;
    if (driverReady)
        usb_serial_jtag_set_select_notif_callback(notification);
    return driverReady;
}
} // namespace
bool usbLinkBegin() {
    owner = xTaskGetCurrentTaskHandle();
    connected = usb_serial_jtag_is_connected();
    if (!installDriver())
        return false;
    initError = esp_register_freertos_tick_hook_for_cpu(connectionTick, 0);
    if (initError != ESP_OK) {
        usb_serial_jtag_set_select_notif_callback(nullptr);
        usb_serial_jtag_driver_uninstall();
        driverReady = false;
        return false;
    }
    session = {true, false, coreNewSession()};
    ready = true;
    return true;
}
void usbLinkMaintain() {
    if (!ready)
        return;
    const auto link = connectionState();
    const auto generation = link.generation;
    if (generation != observedGeneration) {
        coreDisconnect(session);
        session = {true, false, coreNewSession()};
        parser.reset();
        tx.clear();
        txOffset = rxOffset = rxSize = 0;
        observedGeneration = generation;
        // Drop driver-owned bytes as well as our current frame. Otherwise old RX
        // could execute, or old TX could leak, after a rapid unplug/replug.
        if (driverReady) {
            usb_serial_jtag_set_select_notif_callback(nullptr);
            initError = usb_serial_jtag_driver_uninstall();
            driverReady = false;
        }
        if (!link.connected)
            ++disconnects;
        else
            installDriver();
    }
    if (!link.connected || !driverReady)
        return;
    // At most one request and 256 RX/TX bytes per pass. A partial TX frame owns
    // the channel until complete; no event or response can interleave its bytes.
    if (tx.empty()) {
        flint::Frame reply;
        if (coreTakeReply(session, reply))
            tx = flint::encodeFrame(reply);
    }
    if (tx.empty()) {
        bool handled = false;
        if (rxOffset == rxSize) {
            const auto count = usb_serial_jtag_read_bytes(rx.data(), rx.size(), 0);
            rxSize = count > 0 ? size_t(count) : 0;
            rxOffset = 0;
            rxBytes += rxSize;
        }
        while (rxOffset < rxSize && !handled && tx.empty()) {
            parser.feed(std::span(rx).subspan(rxOffset++, 1), [&](flint::Frame frame) {
                handled = true;
                // Every hello starts a new logical client, even if the cable stayed attached.
                if (frame.method == flint::Method::hello) {
                    coreDisconnect(session);
                    session = {true, false, coreNewSession()};
                }
                if (auto reply = coreRequest(frame, session))
                    tx = flint::encodeFrame(*reply);
                txOffset = 0;
            });
        }
        if (tx.empty()) {
            flint::Frame event;
            if (coreTakeCompanionEvent(session, event))
                tx = flint::encodeFrame(event);
        }
    }
    // The generation may change while a request is executing. Never emit its
    // response into the replacement physical session.
    if (generation != connectionState().generation) {
        coreWake(CoreWakeUsbSession);
        return;
    }
    if (!tx.empty()) {
        const auto count = std::min<size_t>(256, tx.size() - txOffset);
        const auto written = usb_serial_jtag_write_bytes(tx.data() + txOffset, count, 0);
        if (written > 0) {
            txOffset += written;
            txBytes += written;
            if (txOffset == tx.size()) {
                tx.clear();
                txOffset = 0;
            }
            coreWake(CoreWakeWork);
        } else if (written < 0) {
            ++txErrors;
        }
        // Zero means backpressure. The driver's TX callback will wake the owner.
    }
    if (tx.empty() && (rxOffset < rxSize || usb_serial_jtag_read_ready()))
        coreWake(CoreWakeWork);
}
void usbLinkDiagnostics(flint::Values &values) {
    values["usb_rx_bytes"] = std::to_string(rxBytes);
    values["usb_tx_bytes"] = std::to_string(txBytes);
    values["usb_tx_errors"] = std::to_string(txErrors);
    values["usb_disconnects"] = std::to_string(disconnects);
    values["usb_sof_glitches"] = std::to_string(connectionState().glitches);
    values["usb_rx_capacity_bytes"] = std::to_string(((flint::MaxPayload + 40 + 63) / 64) * 64);
    values["usb_init_error"] = std::to_string(initError);
}
uint64_t usbLinkNextWake() {
    if (!ready || !driverReady || !connectionState().connected || !tx.empty())
        return UINT64_MAX;
    return coreReplyReady(session) ? 0 : coreCompanionNextWake(session);
}
