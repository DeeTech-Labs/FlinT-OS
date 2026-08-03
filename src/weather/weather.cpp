#include "weather.h"
#include "src/config/config.h"
#include "src/config/debug.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static WeatherState gWeather = {false, false, 0, 0, 0};
static SemaphoreHandle_t gWxMutex = nullptr;
static TaskHandle_t gWxTask = nullptr;

static void wxLock() {
  if (gWxMutex) xSemaphoreTake(gWxMutex, portMAX_DELAY);
}

static void wxUnlock() {
  if (gWxMutex) xSemaphoreGive(gWxMutex);
}

const WeatherState weatherGet() {
  WeatherState snap;
  wxLock();
  snap = gWeather;
  wxUnlock();
  return snap;
}

bool weatherIsOk() {
  wxLock();
  bool ok = gWeather.ok;
  wxUnlock();
  return ok;
}

uint32_t weatherLastFetchMs() {
  wxLock();
  uint32_t ms = gWeather.updatedMs;
  wxUnlock();
  return ms;
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

static bool parseWeatherJson(const String& body, float* tempOut, int* codeOut) {
  int cur = body.indexOf("\"current\"");
  if (cur < 0) return false;

  int tKey = body.indexOf("\"temperature_2m\":", cur);
  if (tKey < 0) return false;
  *tempOut = body.substring(tKey + 17).toFloat();

  int cKey = body.indexOf("\"weather_code\":", cur);
  if (cKey < 0) return false;
  *codeOut = body.substring(cKey + 15).toInt();
  return true;
}

static bool weatherFetchOnce() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  wxLock();
  gWeather.loading = true;
  wxUnlock();

  String url = String("http://api.open-meteo.com/v1/forecast?latitude=")
               + String(WEATHER_LAT, 4)
               + "&longitude=" + String(WEATHER_LON, 4)
               + "&current=temperature_2m,weather_code"
               + "&timezone=" + WEATHER_TZ;

  LOGI("wx", "async GET lat=%.4f lon=%.4f", WEATHER_LAT, WEATHER_LON);
  LOGD("wx", "url %s", url.c_str());

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) {
    LOGE("wx", "http.begin failed");
    wxLock();
    gWeather.loading = false;
    wxUnlock();
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    LOGE("wx", "HTTP %d", code);
    http.end();
    wxLock();
    gWeather.loading = false;
    wxUnlock();
    return false;
  }

  String body = http.getString();
  http.end();

  float t = 0;
  int w = 0;
  if (!parseWeatherJson(body, &t, &w)) {
    LOGE("wx", "JSON parse failed");
    wxLock();
    gWeather.loading = false;
    wxUnlock();
    return false;
  }

  wxLock();
  gWeather.tempC = t;
  gWeather.code = w;
  gWeather.ok = true;
  gWeather.loading = false;
  gWeather.updatedMs = millis();
  wxUnlock();

  LOGI("wx", "ok temp=%.1fC code=%d (%s)", t, w, weatherCodeRu(w));
  return true;
}

static void weatherTask(void* /*arg*/) {
  LOGI("wx", "service started");
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      wxLock();
      bool need = !gWeather.ok
                  || (millis() - gWeather.updatedMs) > WEATHER_REFRESH_MS;
      bool busy = gWeather.loading;
      wxUnlock();

      if (need && !busy) {
        weatherFetchOnce();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void weatherStartService() {
  if (!gWxMutex) {
    gWxMutex = xSemaphoreCreateMutex();
  }
  if (!gWxTask) {
    xTaskCreatePinnedToCore(
        weatherTask, "weather", 8192, nullptr, 1, &gWxTask, 0);
  }
}
