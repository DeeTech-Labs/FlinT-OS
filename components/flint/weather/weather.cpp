#include "weather.h"
#include "config/config.h"
#include "config/debug.h"
#include "net/wifi_net.h"
#include "settings/settings.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static WeatherState gWeather = {false, false, 0, 0, 0};
static WeatherState gWeatherSnap = {false, false, 0, 0, 0};
static SemaphoreHandle_t gWxMutex = nullptr;
static TaskHandle_t gWxTask = nullptr;

static uint32_t gNextAttemptMs = 0;
static uint32_t gFailBackoffMs = 5000;

static void wxLock() {
  if (gWxMutex) xSemaphoreTake(gWxMutex, portMAX_DELAY);
}

static void wxUnlock() {
  if (gWxMutex) xSemaphoreGive(gWxMutex);
}

static void wxPublishLocked() {
  gWeatherSnap = gWeather;
  uiMarkDirty(UI_DIRTY_WEATHER);
}

const WeatherState weatherGet() {
  WeatherState snap;
  if (gWxMutex && xSemaphoreTake(gWxMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
    snap = gWeather;
    xSemaphoreGive(gWxMutex);
    return snap;
  }
  return gWeatherSnap;
}

bool weatherIsOk() {
  return weatherGet().ok;
}

uint32_t weatherLastFetchMs() {
  return weatherGet().updatedMs;
}

const char* weatherCodeRu(int code) {
  if (code == 0) return "ясно";
  if (code <= 3) return "облачно";
  if (code == 45 || code == 48) return "туман";
  if (code >= 51 && code <= 57) return "морось";
  if (code >= 61 && code <= 67) return "дождь";
  if (code >= 71 && code <= 77) return "снег";
  if (code >= 80 && code <= 82) return "ливень";
  if (code >= 85 && code <= 86) return "снег";
  if (code >= 95) return "гроза";
  return "погода";
}

static bool parseWeatherJson(const char* body, float* tempOut, int* codeOut) {
  const char* cur = strstr(body, "\"current\"");
  if (!cur) return false;

  const char* tKey = strstr(cur, "\"temperature_2m\":");
  if (!tKey) return false;
  *tempOut = strtof(tKey + 17, nullptr);

  const char* cKey = strstr(cur, "\"weather_code\":");
  if (!cKey) return false;
  *codeOut = static_cast<int>(strtol(cKey + 15, nullptr, 10));
  return true;
}

struct HttpBuf {
  char* data;
  int len;
  int cap;
};

static esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
  auto* buf = static_cast<HttpBuf*>(evt->user_data);
  if (!buf) return ESP_OK;

  if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data && evt->data_len > 0) {
    if (buf->len + evt->data_len + 1 > buf->cap) {
      int ncap = buf->cap ? buf->cap * 2 : 1024;
      while (ncap < buf->len + evt->data_len + 1) ncap *= 2;
      char* nd = static_cast<char*>(realloc(buf->data, ncap));
      if (!nd) return ESP_FAIL;
      buf->data = nd;
      buf->cap = ncap;
    }
    memcpy(buf->data + buf->len, evt->data, evt->data_len);
    buf->len += evt->data_len;
    buf->data[buf->len] = '\0';
  }
  return ESP_OK;
}

static bool weatherFetchOnce() {
  if (!wifiNetIsConnected()) {
    return false;
  }

  wxLock();
  gWeather.loading = true;
  wxPublishLocked();
  wxUnlock();

  const FlintSettings& s = settingsGet();
  char url[192];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code&timezone=%s",
           s.wxLat, s.wxLon, WEATHER_TZ);

  LOGI("wx", "async GET lat=%.4f lon=%.4f", s.wxLat, s.wxLon);
  LOGD("wx", "url %s", url);

  HttpBuf buf{nullptr, 0, 0};
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.method = HTTP_METHOD_GET;
  cfg.timeout_ms = 2500;
  cfg.event_handler = httpEventHandler;
  cfg.user_data = &buf;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    LOGE("wx", "http init failed");
    wxLock();
    gWeather.loading = false;
    wxPublishLocked();
    wxUnlock();
    return false;
  }

  vTaskDelay(pdMS_TO_TICKS(1));
  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  vTaskDelay(pdMS_TO_TICKS(1));

  if (err != ESP_OK || status != 200 || !buf.data) {
    LOGE("wx", "HTTP err=%s status=%d", esp_err_to_name(err), status);
    free(buf.data);
    wxLock();
    gWeather.loading = false;
    wxPublishLocked();
    wxUnlock();
    return false;
  }

  float t = 0;
  int w = 0;
  if (!parseWeatherJson(buf.data, &t, &w)) {
    LOGE("wx", "JSON parse failed");
    free(buf.data);
    wxLock();
    gWeather.loading = false;
    wxPublishLocked();
    wxUnlock();
    return false;
  }
  free(buf.data);

  wxLock();
  gWeather.tempC = t;
  gWeather.code = w;
  gWeather.ok = true;
  gWeather.loading = false;
  gWeather.updatedMs = flintMillis();
  wxPublishLocked();
  wxUnlock();

  LOGI("wx", "ok temp=%.1fC code=%d (%s)", t, w, weatherCodeRu(w));
  return true;
}

static void weatherTask(void* /*arg*/) {
  LOGI("wx", "service started");
  for (;;) {
    const uint32_t now = flintMillis();
    if (wifiNetIsConnected() && (int32_t)(now - gNextAttemptMs) >= 0) {
      wxLock();
      bool need = !gWeather.ok
                  || (now - gWeather.updatedMs) > WEATHER_REFRESH_MS;
      bool busy = gWeather.loading;
      wxUnlock();

      if (need && !busy) {
        const bool ok = weatherFetchOnce();
        if (ok) {
          gFailBackoffMs = 5000;
          gNextAttemptMs = flintMillis() + WEATHER_REFRESH_MS;
        } else {
          gNextAttemptMs = flintMillis() + gFailBackoffMs;
          if (gFailBackoffMs < 120000UL) {
            gFailBackoffMs *= 2;
          }
          LOGI("wx", "retry in %lu ms", (unsigned long)gFailBackoffMs);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void weatherStartService() {
  if (!gWxMutex) {
    gWxMutex = xSemaphoreCreateMutex();
  }
  if (!gWxTask) {
    // Low priority so HTTP never starves the face redraw loop.
    xTaskCreate(weatherTask, "weather", 8192, nullptr, 1, &gWxTask);
  }
}
