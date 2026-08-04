#include "wifi_net.h"
#include "config/config.h"
#include "config/debug.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t s_wifi_events = nullptr;
static esp_netif_t* s_sta = nullptr;
static bool s_started = false;

static const int WIFI_OK_BIT = BIT0;

static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
  (void)arg;
  (void)data;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(s_wifi_events, WIFI_OK_BIT);
    uiMarkDirty(UI_DIRTY_WIFI);
    LOGI("wifi", "disconnected — will retry");
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_events, WIFI_OK_BIT);
    uiMarkDirty(UI_DIRTY_WIFI);
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(data);
    LOGI("wifi", "up ip=" IPSTR " rssi pending", IP2STR(&event->ip_info.ip));
  }
}

bool wifiNetIsConnected() {
  if (!s_wifi_events) {
    return false;
  }
  return (xEventGroupGetBits(s_wifi_events) & WIFI_OK_BIT) != 0;
}

void wifiNetBegin() {
  if (s_started) {
    return;
  }
  LOGI("wifi", "begin SSID=\"%s\"", WIFI_SSID);

  s_wifi_events = xEventGroupCreate();
  s_sta = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, nullptr, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, nullptr, nullptr));

  wifi_config_t wifi_config = {};
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS,
          sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  // Modem sleep stalls UI on C3 while radio wakes.
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  s_started = true;
}

bool wifiNetWaitConnected(uint32_t timeoutMs) {
  if (!s_wifi_events) {
    return false;
  }
  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_events, WIFI_OK_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeoutMs));
  if ((bits & WIFI_OK_BIT) == 0) {
    LOGE("wifi", "wait timeout");
    return false;
  }

  wifi_ap_record_t ap{};
  int8_t rssi = 0;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    rssi = ap.rssi;
  }
  esp_netif_ip_info_t ip{};
  esp_netif_get_ip_info(s_sta, &ip);
  LOGI("wifi", "connected ip=" IPSTR " rssi=%d", IP2STR(&ip.ip), (int)rssi);
  return true;
}

void wifiNetMaintain() {
  static bool wasConnected = false;
  bool now = wifiNetIsConnected();

  if (now && !wasConnected) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    wifi_ap_record_t ap{};
    int8_t rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      rssi = ap.rssi;
    }
    esp_netif_ip_info_t ip{};
    if (s_sta) {
      esp_netif_get_ip_info(s_sta, &ip);
    }
    LOGI("wifi", "up ip=" IPSTR " rssi=%d", IP2STR(&ip.ip), (int)rssi);
  }
  if (!now && wasConnected) {
    LOGE("wifi", "down");
  }
  wasConnected = now;

  if (now) {
    return;
  }

  static uint32_t lastTry = 0;
  if (flintMillis() - lastTry > WIFI_RETRY_MS) {
    lastTry = flintMillis();
    LOGI("wifi", "reconnect()");
    esp_wifi_connect();
  }
}
