#include "wifi_net.h"
#include "config/config.h"
#include "config/debug.h"
#include "event/event.h"
#include "util/flint_sys.h"

#include <limits.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t s_wifi_events = nullptr;
static esp_netif_t* s_sta = nullptr;
static bool s_started = false;
static bool s_connecting = false;
static uint32_t s_connectStartedMs = 0;
static uint32_t s_lastReconnectMs = 0;

static const int WIFI_OK_BIT = BIT0;

static void wifiMarkConnecting() {
  s_connecting = true;
  s_connectStartedMs = flintMillis() ? flintMillis() : 1;
}

static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
  (void)arg;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    wifiMarkConnecting();
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(s_wifi_events, WIFI_OK_BIT);
    s_connecting = false;
    flintEventEmit(FLINT_EVT_WIFI);

    // Immediate retry on drop — do NOT also reconnect from maintain() while in-flight.
    const uint32_t now = flintMillis();
    if (now - s_lastReconnectMs >= 2000UL) {
      s_lastReconnectMs = now ? now : 1;
      // Brief full-radio while associating; MIN_MODEM resumes on GOT_IP.
      esp_wifi_set_ps(WIFI_PS_NONE);
      wifiMarkConnecting();
      LOGI("wifi", "disconnected — reconnect");
      esp_wifi_connect();
    } else {
      LOGI("wifi", "disconnected — backoff");
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    s_connecting = false;
    // Modem sleep after link is up — keeps association on a wall clock.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    xEventGroupSetBits(s_wifi_events, WIFI_OK_BIT);
    flintEventEmit(FLINT_EVT_WIFI);
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(data);
    LOGI("wifi", "up ip=" IPSTR " ps=min_modem", IP2STR(&event->ip_info.ip));
  }
}

bool wifiNetIsConnected() {
  if (!s_wifi_events) {
    return false;
  }
  return (xEventGroupGetBits(s_wifi_events) & WIFI_OK_BIT) != 0;
}

int8_t wifiNetRssi() {
  if (!wifiNetIsConnected()) {
    return INT8_MIN;
  }
  wifi_ap_record_t ap{};
  if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
    return INT8_MIN;
  }
  return ap.rssi;
}

uint8_t wifiNetBars() {
  if (!wifiNetIsConnected()) {
    return 0;
  }
  const int8_t rssi = wifiNetRssi();
  if (rssi == INT8_MIN) {
    return 1;  // associated but RSSI unread — show weakest connected glyph
  }
  // Typical indoor thresholds for a 3-bar status glyph.
  if (rssi >= -55) {
    return 3;
  }
  if (rssi >= -67) {
    return 2;
  }
  return 1;
}

void wifiNetBegin() {
  if (s_started) {
    return;
  }
  LOGI("wifi", "begin SSID=\"%s\"", WIFI_SSID);

  // Quiet driver noise (FT-PSK fallback, etc.); app logs stay on LOGI.
  esp_log_level_set("wifi", ESP_LOG_ERROR);

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
  // Prefer stable WPA2; avoid FT/WPA3 negotiation surprises on home APs.
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  // Full radio only while associating; GOT_IP switches to MIN_MODEM.
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
  static uint8_t lastBars = 0xFF;
  static uint32_t lastRssiSampleMs = 0;
  bool now = wifiNetIsConnected();
  const bool rose = now && !wasConnected;

  if (rose) {
    // Rising edge: ensure modem sleep even if GOT_IP handler raced.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    wifi_ap_record_t ap{};
    int8_t rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      rssi = ap.rssi;
    }
    esp_netif_ip_info_t ip{};
    if (s_sta) {
      esp_netif_get_ip_info(s_sta, &ip);
    }
    LOGI("wifi", "up ip=" IPSTR " rssi=%d ps=min_modem", IP2STR(&ip.ip), (int)rssi);
  }
  if (!now && wasConnected) {
    LOGE("wifi", "down");
  }
  wasConnected = now;

  // Poll RSSI on link-up and every few seconds; emit when bar count changes.
  uint8_t bars = 0;
  if (now) {
    const uint32_t t = flintMillis();
    if (rose || lastBars == 0xFF || (t - lastRssiSampleMs) >= 5000UL) {
      lastRssiSampleMs = t ? t : 1;
      bars = wifiNetBars();
    } else {
      bars = lastBars;
    }
  }
  if (bars != lastBars) {
    lastBars = bars;
    flintEventEmit(FLINT_EVT_WIFI);
  }

  if (now) {
    s_connecting = false;
    return;
  }

  // Association / DHCP still in progress — do not call esp_wifi_connect() again
  // (causes "sta is connected, disconnect before connecting").
  if (s_connecting) {
    const uint32_t elapsed = flintMillis() - s_connectStartedMs;
    if (elapsed < WIFI_CONNECT_TIMEOUT_MS) {
      return;
    }
    LOGI("wifi", "connect timeout — retry");
    s_connecting = false;
  }

  const uint32_t nowMs = flintMillis();
  if (nowMs - s_lastReconnectMs < WIFI_RETRY_MS) {
    return;
  }
  s_lastReconnectMs = nowMs ? nowMs : 1;
  esp_wifi_set_ps(WIFI_PS_NONE);
  wifiMarkConnecting();
  LOGI("wifi", "reconnect()");
  esp_wifi_connect();
}
