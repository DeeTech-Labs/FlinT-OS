#include "weather.h"
#include "config/config.h"
#include "config/debug.h"
#include "event/event.h"
#include "i18n/i18n.h"
#include "net/wifi_net.h"
#include "settings/settings.h"
#include "time/tz_db.h"
#include "util/flint_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static WeatherState gWeather{};
static WeatherState gWeatherSnap{};
static SemaphoreHandle_t gWxMutex = nullptr;
static TaskHandle_t gWxTask = nullptr;

static uint32_t gNextAttemptMs = 0;
static uint32_t gFailBackoffMs = 5000;

static constexpr int kHttpCapMax = 12288;

static void wxLock() {
  if (gWxMutex) xSemaphoreTake(gWxMutex, portMAX_DELAY);
}

static void wxUnlock() {
  if (gWxMutex) xSemaphoreGive(gWxMutex);
}

static void wxPublishLocked() {
  gWeatherSnap = gWeather;
  flintEventEmit(FLINT_EVT_WEATHER);
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

const char* weatherCodeLabel(int code) {
  return flintTrWeatherCode(code);
}

const char* weatherCodeRu(int code) {
  return weatherCodeLabel(code);
}

static const char* findArrayAfterKey(const char* body, const char* key) {
  const char* k = strstr(body, key);
  if (!k) return nullptr;
  const char* br = strchr(k, '[');
  return br;
}

static bool parseFloatList(const char* arr, float* out, int maxN, int* got) {
  if (!arr || !out || maxN <= 0) return false;
  *got = 0;
  const char* p = arr + 1;
  while (*p && *got < maxN) {
    while (*p == ' ' || *p == ',') p++;
    if (*p == ']') break;
    char* end = nullptr;
    float v = strtof(p, &end);
    if (end == p) break;
    out[(*got)++] = v;
    p = end;
  }
  return *got > 0;
}

static bool parseIntList(const char* arr, int* out, int maxN, int* got) {
  if (!arr || !out || maxN <= 0) return false;
  *got = 0;
  const char* p = arr + 1;
  while (*p && *got < maxN) {
    while (*p == ' ' || *p == ',') p++;
    if (*p == ']') break;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p) break;
    out[(*got)++] = static_cast<int>(v);
    p = end;
  }
  return *got > 0;
}

static bool parseWeatherJson(const char* body, WeatherState* out) {
  const char* cur = strstr(body, "\"current\"");
  if (!cur) return false;

  const char* tKey = strstr(cur, "\"temperature_2m\":");
  if (!tKey) return false;
  out->tempC = strtof(tKey + 17, nullptr);

  const char* cKey = strstr(cur, "\"weather_code\":");
  if (!cKey) return false;
  out->code = static_cast<int>(strtol(cKey + 15, nullptr, 10));

  out->dailyOk = false;
  out->hourlyCount = 0;

  const char* daily = strstr(body, "\"daily\"");
  if (daily) {
    float tMax[2]{};
    float tMin[2]{};
    int codes[2]{};
    int nMax = 0, nMin = 0, nCode = 0;
    const char* aMax = findArrayAfterKey(daily, "\"temperature_2m_max\"");
    const char* aMin = findArrayAfterKey(daily, "\"temperature_2m_min\"");
    const char* aCode = findArrayAfterKey(daily, "\"weather_code\"");
    if (aMax && aMin && aCode && parseFloatList(aMax, tMax, 2, &nMax)
        && parseFloatList(aMin, tMin, 2, &nMin) && parseIntList(aCode, codes, 2, &nCode)
        && nMax > 0 && nMin > 0 && nCode > 0) {
      out->daily.tMaxCx10 = static_cast<int16_t>(tMax[0] * 10.0f);
      out->daily.tMinCx10 = static_cast<int16_t>(tMin[0] * 10.0f);
      out->daily.code = static_cast<int16_t>(codes[0]);
      out->dailyOk = true;
    }
  }

  const char* hourly = strstr(body, "\"hourly\"");
  if (hourly) {
    float temps[24]{};
    int codes[24]{};
    int nT = 0, nC = 0;
    const char* aT = findArrayAfterKey(hourly, "\"temperature_2m\"");
    const char* aC = findArrayAfterKey(hourly, "\"weather_code\"");
    if (aT && aC && parseFloatList(aT, temps, 24, &nT) && parseIntList(aC, codes, 24, &nC)) {
      const int n = nT < nC ? nT : nC;
      uint8_t count = 0;
      for (int h = 0; h < 24 && count < 8; h += 3) {
        if (h >= n) break;
        out->hourly[count].hour = static_cast<int8_t>(h);
        out->hourly[count].tempCx10 = static_cast<int16_t>(temps[h] * 10.0f);
        out->hourly[count].code = static_cast<int16_t>(codes[h]);
        count++;
      }
      out->hourlyCount = count;
    }
  }

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
      int ncap = buf->cap ? buf->cap * 2 : 2048;
      while (ncap < buf->len + evt->data_len + 1) ncap *= 2;
      if (ncap > kHttpCapMax) ncap = kHttpCapMax;
      if (buf->len + evt->data_len + 1 > ncap) {
        return ESP_FAIL;
      }
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
  char tzEnc[48];
  tzDbUrlEncodeId(s.tzId, tzEnc, sizeof(tzEnc));

  char url[288];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&hourly=temperature_2m,weather_code&forecast_days=1&timezone=%s",
           s.wxLat, s.wxLon, tzEnc);

  LOGI("wx", "async GET lat=%.4f lon=%.4f tz=%s", s.wxLat, s.wxLon, s.tzId);

  HttpBuf buf{nullptr, 0, 0};
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.method = HTTP_METHOD_GET;
  cfg.timeout_ms = 8000;
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

  WeatherState parsed{};
  if (!parseWeatherJson(buf.data, &parsed)) {
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
  gWeather.tempC = parsed.tempC;
  gWeather.code = parsed.code;
  gWeather.dailyOk = parsed.dailyOk;
  gWeather.daily = parsed.daily;
  gWeather.hourlyCount = parsed.hourlyCount;
  memcpy(gWeather.hourly, parsed.hourly, sizeof(parsed.hourly));
  gWeather.ok = true;
  gWeather.loading = false;
  gWeather.updatedMs = flintMillis();
  wxPublishLocked();
  wxUnlock();

  LOGI("wx", "ok temp=%.1fC code=%d daily=%d hourly=%u",
       parsed.tempC, parsed.code, (int)parsed.dailyOk, (unsigned)parsed.hourlyCount);
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
    xTaskCreate(weatherTask, "weather", 10240, nullptr, 1, &gWxTask);
  }
}

void weatherRequestRefresh() {
  wxLock();
  gWeather.ok = false;
  gWeather.updatedMs = 0;
  wxUnlock();
  gNextAttemptMs = 0;
  gFailBackoffMs = 5000;
}
