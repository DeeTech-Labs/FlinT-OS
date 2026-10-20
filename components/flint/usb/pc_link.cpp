#include "usb/pc_link.h"
#include "ble/ble_link.h"
#include "shell/shell.h"
#include "companion/control.h"
#include "companion/action_input.h"
#include "usb/pc_protocol.h"
#include "usb/pc_data.h"
#include "usb/status_pattern.h"
#include "config/config.h"
#include "config/debug.h"
#include "event/event.h"
#include "time/clock_time.h"
#include "util/flint_sys.h"
#include "settings/settings.h"
#include "display/display.h"
#include "esp_mac.h"

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

namespace {
bool gConnected = false;
bool gLedReady = false;
uint32_t gFindStarted = 0;
bool gFinding = false;
PcProtocol gProtocol;
PcStatusPattern gPattern;

void sendInfo(char* response, size_t capacity, bool trustedTransport) {
  uint8_t mac[6]{};
  if (esp_efuse_mac_get_default(mac) != ESP_OK) {
    snprintf(response, capacity, "FLINT ERROR device id unavailable\n");
    return;
  }
  char id[13];
  snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  const PcDeviceInfo info{id, "Spark", FLINT_BOARD_ID, FLINT_IDF_TARGET,
                          FLINT_OS_VERSION_STRING, FLINT_OS_API_VERSION, SCR_W, SCR_H, trustedTransport};
  char json[512];
  if (pcWriteInfo(json, sizeof(json), info)) snprintf(response, capacity, "FLINT INFO %s\n", json);
  else snprintf(response, capacity, "FLINT ERROR info overflow\n");
}

void sendStatus(char* response, size_t capacity) {
  const FlintSettings& settings = settingsGet();
  const PcDeviceStatus status{esp_timer_get_time() / 1000, flintFreeHeap(), gConnected,
      clockTimeIsValid(), clockTimeIsPcSynced(), clockTimeGetEpoch(), settings.tzId,
      settings.activeFace, settings.showSeconds, settings.lang, settings.hourFormat,
      settings.dateFormat, settings.dateSep};
  char json[768];
  if (pcWriteStatus(json, sizeof(json), status)) snprintf(response, capacity, "FLINT STATUS %s\n", json);
  else snprintf(response, capacity, "FLINT ERROR status overflow\n");
}

void updateLed() {
  if (!gLedReady) return;
  const auto now = flintMillis();
  if (gFinding && now - gFindStarted >= 5000) gFinding = false;
  const bool on = gFinding ? (now - gFindStarted) % 200 < 100 : gPattern.on(gConnected,now);
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
  if (command.kind == PcCommandKind::None) return true;
  char response[896]{};
  if (command.kind == PcCommandKind::Request && !strcmp(command.operation, "PAIR")) {
    char json[672]{};
    if (bleLinkPairingCommand(command.payload, json, sizeof(json)))
      snprintf(response, sizeof(response), "FLINT RESULT {\"id\":%lu,\"ok\":true,\"result\":%s}\n", static_cast<unsigned long>(command.requestId), json);
    else snprintf(response, sizeof(response), "FLINT RESULT {\"id\":%lu,\"ok\":false,\"error\":\"pairing_command_failed\"}\n", static_cast<unsigned long>(command.requestId));
  } else if (command.kind == PcCommandKind::Request && !strcmp(command.operation, "KEY")) {
    char key = !strcmp(command.payload,"enter") ? '\r' : !strcmp(command.payload,"settings") ? 's'
      : !strcmp(command.payload,"next") ? 'n' : !strcmp(command.payload,"previous") ? 'p'
      : !strcmp(command.payload,"back") ? 'q' : !strcmp(command.payload,"plus") ? '+' : !strcmp(command.payload,"minus") ? '-' : 0;
    if (key) shellHandleKey(key);
    snprintf(response, sizeof(response), "FLINT RESULT {\"id\":%lu,\"ok\":%s,%s}\n", static_cast<unsigned long>(command.requestId),
      key ? "true" : "false", key ? "\"result\":{\"emulated\":true}" : "\"error\":\"invalid_key\"");
  } else if (command.kind == PcCommandKind::Request && !strcmp(command.operation, "BUTTON")) {
    const bool okay = flintActionInput.push(command.payload);
    snprintf(response, sizeof(response), "FLINT RESULT {\"id\":%lu,\"ok\":%s,%s}\n", static_cast<unsigned long>(command.requestId),
      okay ? "true" : "false", okay ? "\"result\":{\"emulated\":true}" : "\"error\":\"invalid_button\"");
  } else pcLinkExecute(command, response, sizeof(response), true);
  if (response[0]) printf("%s", response);
  return true;
}

void pcLinkExecute(const PcCommand& command, char* response, size_t capacity, bool trustedTransport) {
  if (!response || capacity == 0) return;
  response[0] = '\0';
  switch (command.kind) {
    case PcCommandKind::Request:
      companionExecute(command, response, capacity, trustedTransport);
      break;
    case PcCommandKind::Hello:
      snprintf(response, capacity, "FLINT HELLO 1 %s\n", FLINT_BOARD_ID);
      break;
    case PcCommandKind::Info:
      sendInfo(response, capacity, trustedTransport);
      break;
    case PcCommandKind::Status:
      sendStatus(response, capacity);
      break;
    case PcCommandKind::Time:
      if (clockTimeSyncFromPc(static_cast<time_t>(command.epoch))) {
        flintEventEmit(FLINT_EVT_TIME_VALID);
        gPattern.synced(flintMillis());
        updateLed();
        snprintf(response, capacity, "FLINT TIME OK %lld\n", static_cast<long long>(command.epoch));
        LOGI("usb", "time synced; LED 3 flashes");
        break;
      }
      [[fallthrough]];
    case PcCommandKind::Error:
      snprintf(response, capacity, "FLINT ERROR invalid command\n");
      break;
    case PcCommandKind::None:
      break;
  }
}

bool pcLinkFind() { if (!gLedReady) return false; gFinding = true; gFindStarted = flintMillis(); updateLed(); return true; }
