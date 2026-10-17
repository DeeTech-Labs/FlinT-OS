#include "usb/pc_link.h"
#include "usb/pc_protocol.h"
#include "usb/status_pattern.h"
#include "config/config.h"
#include "config/debug.h"
#include "event/event.h"
#include "time/clock_time.h"
#include "util/flint_sys.h"

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

namespace {
bool gConnected = false;
bool gLedReady = false;
PcProtocol gProtocol;
PcStatusPattern gPattern;

void updateLed() {
  if (!gLedReady) return;
  const bool on = gPattern.on(gConnected, flintMillis());
  gpio_set_level(static_cast<gpio_num_t>(FLINT_STATUS_LED_GPIO),
                 FLINT_STATUS_LED_ACTIVE_LOW ? !on : on);
}
}  // namespace

void pcLinkBegin() {
  if (FLINT_STATUS_LED_GPIO >= 0) {
    const auto pin = static_cast<gpio_num_t>(FLINT_STATUS_LED_GPIO);
    ESP_ERROR_CHECK(gpio_set_level(pin, FLINT_STATUS_LED_ACTIVE_LOW ? 1 : 0));
    ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_OUTPUT));
    gLedReady = true;
  }
  pcLinkMaintain();
}

void pcLinkMaintain() {
  // SOF packets distinguish an actual USB host from a USB charger/power bank.
  const bool connected = usb_serial_jtag_is_connected();
  if (connected != gConnected) {
    gConnected = connected;
    gProtocol.reset();
    LOGI("usb", "%s", connected ? "PC connected" : "PC disconnected");
    flintEventEmit(FLINT_EVT_USB);
  }
  updateLed();
}

bool pcLinkConnected() { return gConnected; }

bool pcLinkHandleByte(char c) {
  PcCommand command;
  if (!gProtocol.feed(c, flintMillis(), command)) return false;
  switch (command.kind) {
    case PcCommandKind::Hello:
      printf("FLINT HELLO 1 %s\n", FLINT_BOARD_ID);
      break;
    case PcCommandKind::Time:
      if (clockTimeSyncFromPc(static_cast<time_t>(command.epoch))) {
        flintEventEmit(FLINT_EVT_TIME_VALID);
        gPattern.synced(flintMillis());
        updateLed();
        printf("FLINT TIME OK %lld\n", static_cast<long long>(command.epoch));
        LOGI("usb", "time synced; LED 3 flashes");
        break;
      }
      [[fallthrough]];
    case PcCommandKind::Error:
      printf("FLINT ERROR invalid command\n");
      break;
    case PcCommandKind::None:
      break;
  }
  return true;
}
