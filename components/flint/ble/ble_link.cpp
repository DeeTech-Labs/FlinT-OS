#include "ble/ble_link.h"
#include "sdkconfig.h"
#if CONFIG_BT_NIMBLE_ENABLED
#if CONFIG_BT_NIMBLE_MAX_BONDS < 1
#error "FlinT BLE requires CONFIG_BT_NIMBLE_MAX_BONDS >= 1 for NimBLE security capacity checks, even with runtime bonding disabled"
#endif
#include "usb/pc_link.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <algorithm>
#include <atomic>
#include <cstring>

// ESP-IDF exports this initializer but omits its declaration from the public header.
extern "C" void ble_store_config_init(void);

// Canonical UUIDs: e7f1000{1,2,3}-7a1e-4b5c-9d2f-46544c494e54
static const ble_uuid128_t serviceUuid = BLE_UUID128_INIT(0x54,0x4e,0x49,0x4c,0x54,0x46,0x2f,0x9d,0x5c,0x4b,0x1e,0x7a,1,0,0xf1,0xe7);
static const ble_uuid128_t rxUuid = BLE_UUID128_INIT(0x54,0x4e,0x49,0x4c,0x54,0x46,0x2f,0x9d,0x5c,0x4b,0x1e,0x7a,2,0,0xf1,0xe7);
static const ble_uuid128_t txUuid = BLE_UUID128_INIT(0x54,0x4e,0x49,0x4c,0x54,0x46,0x2f,0x9d,0x5c,0x4b,0x1e,0x7a,3,0,0xf1,0xe7);
namespace {
struct Request { PcCommand command; uint16_t connection; uint32_t generation; };
std::atomic<const char*> state{"starting"};
std::atomic<int> lastDisconnectReason{0};
std::atomic<int> lastEncryptionStatus{-1};
QueueHandle_t requests;
PcProtocol parser;
std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
std::atomic<uint32_t> generation{0};
std::atomic<bool> subscribed{false};
uint16_t txHandle;
uint8_t addressType;
char response[896]{};
size_t offset = 0;
uint32_t responseGeneration = 0;
std::atomic<bool> inFlight{false};
void advertise();
int gapEvent(ble_gap_event* event, void*) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        ++generation;
        connection = event->connect.conn_handle;
        state = "connected";
        parser.reset();
        lastEncryptionStatus = -2;
        const int securityError = ble_gap_security_initiate(event->connect.conn_handle);
        if (securityError && securityError != BLE_HS_EALREADY) {
          lastEncryptionStatus = securityError;
          ESP_LOGE("ble", "security initiation failed: %d", securityError);
        }
      } else advertise();
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      lastDisconnectReason = event->disconnect.reason;
      ESP_LOGW("ble", "disconnected reason=%d", event->disconnect.reason);
      connection = BLE_HS_CONN_HANDLE_NONE;
      ++generation;
      subscribed = false;
      parser.reset();
      advertise();
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      if (event->subscribe.attr_handle == txHandle) subscribed = event->subscribe.cur_indicate;
      break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      ESP_LOGW("ble", "pairing IO action=%d", event->passkey.params.action);
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      lastEncryptionStatus = event->enc_change.status;
      ESP_LOGW("ble", "encryption status=%d", event->enc_change.status);
      break;
    case BLE_GAP_EVENT_NOTIFY_TX:
      if (event->notify_tx.attr_handle == txHandle && event->notify_tx.indication && event->notify_tx.status != 0) {
        if (event->notify_tx.status != BLE_HS_EDONE) {
          ESP_LOGE("ble", "indication failed: %d", event->notify_tx.status);
          ble_gap_terminate(event->notify_tx.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        inFlight = false;
      }
      break;
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
    default: break;
  }
  return 0;
}
void advertise() {
  ble_hs_adv_fields fields{};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids128 = const_cast<ble_uuid128_t*>(&serviceUuid);
  fields.num_uuids128 = 1;
  fields.uuids128_is_complete = 1;
  const int fieldsError = ble_gap_adv_set_fields(&fields);
  if (fieldsError) { state = "advertisement_fields_failed"; ESP_LOGE("ble", "advertisement fields failed: %d", fieldsError); return; }
  ble_hs_adv_fields scan{};
  scan.name = reinterpret_cast<const uint8_t*>("FlinT Spark");
  scan.name_len = strlen("FlinT Spark");
  scan.name_is_complete = 1;
  const int scanError = ble_gap_adv_rsp_set_fields(&scan);
  if (scanError) { state = "scan_response_failed"; ESP_LOGE("ble", "scan response failed: %d", scanError); return; }
  ble_gap_adv_params params{};
  params.conn_mode = BLE_GAP_CONN_MODE_UND;
  params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  params.itvl_min = 800;
  params.itvl_max = 1200;
  const int error = ble_gap_adv_start(addressType, nullptr, BLE_HS_FOREVER, &params, gapEvent, nullptr);
  if (error) { state = "advertising_start_failed"; ESP_LOGE("ble", "advertising start failed: %d", error); }
  else { state = "advertising"; ESP_LOGW("ble", "advertising FlinT Spark, address type=%u", addressType); }
}
void synced() {
  state = "synced";
  const int addressError = ble_hs_util_ensure_addr(0);
  if (addressError) { state = "address_failed"; ESP_LOGE("ble", "address setup failed: %d", addressError); return; }
  const int inferError = ble_hs_id_infer_auto(0, &addressType);
  if (inferError) { state = "address_selection_failed"; ESP_LOGE("ble", "address selection failed: %d", inferError); return; }
  advertise();
}
int access(uint16_t handle, uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
  if (!subscribed) return BLE_ATT_ERR_INSUFFICIENT_RES;
  char bytes[32];
  uint16_t length = 0;
  if (ble_hs_mbuf_to_flat(ctxt->om, bytes, sizeof(bytes), &length)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  if (!length || length > 20) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  for (unsigned i = 0; i < length; ++i) {
    PcCommand command;
    if (!parser.feed(bytes[i], xTaskGetTickCount() * portTICK_PERIOD_MS, command)) {
      parser.reset(); return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (command.kind != PcCommandKind::None) {
      if (i + 1 != length) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      Request request{command, handle, generation.load()};
      if (xQueueSend(requests, &request, 0) != pdTRUE) return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
  }
  return 0;
}
ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};
void hostTask(void*) { nimble_port_run(); nimble_port_freertos_deinit(); }
}
const char* bleLinkState() { return state.load(); }
int bleLinkLastDisconnectReason() { return lastDisconnectReason.load(); }
int bleLinkLastEncryptionStatus() { return lastEncryptionStatus.load(); }
void bleLinkBegin() {
  // Do not erase NVS automatically: it may contain other product data.
  if (nvs_flash_init() != ESP_OK) { state = "nvs_failed"; ESP_LOGE("ble", "NVS unavailable"); return; }
  requests = xQueueCreate(1, sizeof(Request));
  if (!requests) { state = "queue_failed"; return; }
  const auto initError = nimble_port_init();
  if (initError != ESP_OK) { state = "initialization_failed"; ESP_LOGE("ble", "NimBLE initialization failed: %s", esp_err_to_name(initError)); vQueueDelete(requests); requests = nullptr; return; }
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_gap_device_name_set("FlinT Spark");
  characteristics[0].uuid = &rxUuid.u;
  characteristics[0].access_cb = access;
  characteristics[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC;
  characteristics[1].uuid = &txUuid.u;
  // NimBLE requires an access callback even for an indication-only value.
  characteristics[1].access_cb = access;
  characteristics[1].flags = BLE_GATT_CHR_F_INDICATE | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC;
  characteristics[1].val_handle = &txHandle;
  services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
  services[0].uuid = &serviceUuid.u;
  services[0].characteristics = characteristics;
  const int countError = ble_gatts_count_cfg(services);
  const int serviceError = countError ? 0 : ble_gatts_add_svcs(services);
  if (countError || serviceError) {
    state = "gatt_failed"; ESP_LOGE("ble", "GATT setup failed: count=%d services=%d", countError, serviceError);
    nimble_port_deinit();
    vQueueDelete(requests); requests = nullptr;
    return;
  }
  // Encrypted Just Works for time/settings/design. Private Companion operations require USB.
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_sc = 1;
  // No persistent bonds in this MVP. The reserved RAM slot is required by
  // NimBLE's pre-pairing capacity check, not by negotiated key persistence.
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_our_key_dist = 0;
  ble_hs_cfg.sm_their_key_dist = 0;
  ble_hs_cfg.sync_cb = synced;
  // The security manager probes the key store even when bonding is disabled.
  // Without callbacks it receives ENOTSUP instead of ENOENT and aborts pairing.
  ble_store_config_init();

  state = "host_starting";
  nimble_port_freertos_init(hostTask);
}
void bleLinkMaintain() {
  if (!requests) return;
  if (responseGeneration != generation.load()) {
    offset = 0; response[0] = 0; inFlight = false;
    responseGeneration = generation.load();
  }
  Request request;
  if (!response[0] && xQueueReceive(requests, &request, 0) == pdTRUE) {
    if (request.connection == connection && request.generation == generation && subscribed) {
      responseGeneration = request.generation;
      pcLinkExecute(request.command, response, sizeof(response));
      offset = 0;
    }
  }
  if (!response[0] || inFlight || connection == BLE_HS_CONN_HANDLE_NONE || !subscribed) return;
  const size_t length = strlen(response);
  if (offset == length) { response[0] = 0; return; }
  // 20-byte fragments work even at the default ATT MTU; indications apply backpressure.
  const auto count = std::min<size_t>(20, length - offset);
  auto* packet = ble_hs_mbuf_from_flat(response + offset, count);
  if (!packet) return;
  inFlight = true;
  if (ble_gatts_indicate_custom(connection, txHandle, packet) == 0) offset += count;
  else inFlight = false;
}
#else
const char* bleLinkState() { return "disabled"; }
int bleLinkLastDisconnectReason() { return 0; }
int bleLinkLastEncryptionStatus() { return -1; }
void bleLinkBegin() {}
void bleLinkMaintain() {}
#endif
