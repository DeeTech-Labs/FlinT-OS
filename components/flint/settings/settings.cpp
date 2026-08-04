#include "settings/settings.h"
#include "config/config.h"
#include "config/debug.h"

#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

static FlintSettings gSettings{};
static bool gLoaded = false;

static void settingsApplyDefaults() {
  strncpy(gSettings.activeFace, ACTIVE_FACE_ID, sizeof(gSettings.activeFace) - 1);
  gSettings.activeFace[sizeof(gSettings.activeFace) - 1] = '\0';
  gSettings.showSeconds = SETTINGS_SEC_AUTO;
  gSettings.lang = SETTINGS_LANG_RU;
  gSettings.wxLat = WEATHER_LAT;
  gSettings.wxLon = WEATHER_LON;
}

void settingsLoad() {
  settingsApplyDefaults();

  nvs_handle_t h;
  esp_err_t err = nvs_open("flint", NVS_READONLY, &h);
  if (err != ESP_OK) {
    LOGI("set", "nvs empty — defaults face=%s", gSettings.activeFace);
    gLoaded = true;
    return;
  }

  size_t len = sizeof(gSettings.activeFace);
  if (nvs_get_str(h, "active_face", gSettings.activeFace, &len) != ESP_OK) {
    strncpy(gSettings.activeFace, ACTIVE_FACE_ID, sizeof(gSettings.activeFace) - 1);
  }

  uint8_t u8 = SETTINGS_SEC_AUTO;
  if (nvs_get_u8(h, "show_sec", &u8) == ESP_OK) {
    gSettings.showSeconds = u8;
  }
  u8 = SETTINGS_LANG_RU;
  if (nvs_get_u8(h, "lang", &u8) == ESP_OK) {
    gSettings.lang = u8;
  }

  // floats stored as i32 * 10000
  int32_t i32 = 0;
  if (nvs_get_i32(h, "wx_lat", &i32) == ESP_OK) {
    gSettings.wxLat = i32 / 10000.0f;
  }
  if (nvs_get_i32(h, "wx_lon", &i32) == ESP_OK) {
    gSettings.wxLon = i32 / 10000.0f;
  }

  nvs_close(h);
  gLoaded = true;
  LOGI("set", "loaded face=%s sec=%u lang=%u lat=%.4f lon=%.4f",
       gSettings.activeFace, (unsigned)gSettings.showSeconds, (unsigned)gSettings.lang,
       gSettings.wxLat, gSettings.wxLon);
}

void settingsSave() {
  nvs_handle_t h;
  esp_err_t err = nvs_open("flint", NVS_READWRITE, &h);
  if (err != ESP_OK) {
    LOGE("set", "nvs_open rw failed %s", esp_err_to_name(err));
    return;
  }

  nvs_set_str(h, "active_face", gSettings.activeFace);
  nvs_set_u8(h, "show_sec", gSettings.showSeconds);
  nvs_set_u8(h, "lang", gSettings.lang);
  nvs_set_i32(h, "wx_lat", (int32_t)(gSettings.wxLat * 10000.0f));
  nvs_set_i32(h, "wx_lon", (int32_t)(gSettings.wxLon * 10000.0f));
  nvs_commit(h);
  nvs_close(h);
  LOGI("set", "saved face=%s", gSettings.activeFace);
}

const FlintSettings& settingsGet() {
  if (!gLoaded) {
    settingsLoad();
  }
  return gSettings;
}

bool settingsSetActiveFace(const char* id) {
  if (!id || !id[0]) {
    return false;
  }
  strncpy(gSettings.activeFace, id, sizeof(gSettings.activeFace) - 1);
  gSettings.activeFace[sizeof(gSettings.activeFace) - 1] = '\0';
  settingsSave();
  return true;
}

void settingsSetShowSeconds(uint8_t mode) {
  gSettings.showSeconds = mode;
  settingsSave();
}

void settingsSetLang(uint8_t lang) {
  gSettings.lang = lang;
  settingsSave();
}

void settingsSetWeatherCoords(float lat, float lon) {
  gSettings.wxLat = lat;
  gSettings.wxLon = lon;
  settingsSave();
}

bool settingsResolveShowSeconds(bool dayProfileShowSeconds) {
  const FlintSettings& s = settingsGet();
  if (s.showSeconds == SETTINGS_SEC_AUTO) {
    return dayProfileShowSeconds;
  }
  return s.showSeconds == SETTINGS_SEC_ON;
}
