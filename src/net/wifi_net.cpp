#include "wifi_net.h"
#include "src/config/config.h"
#include "src/config/debug.h"

#include <WiFi.h>

bool wifiNetIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiNetBegin() {
  LOGI("wifi", "begin SSID=\"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

bool wifiNetWaitConnected(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      LOGE("wifi", "wait timeout status=%d", (int)WiFi.status());
      return false;
    }
    delay(50);
  }
  LOGI("wifi", "connected ip=%s rssi=%d",
       WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

void wifiNetMaintain() {
  static bool wasConnected = false;
  bool now = (WiFi.status() == WL_CONNECTED);

  if (now && !wasConnected) {
    LOGI("wifi", "up ip=%s rssi=%d",
         WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!now && wasConnected) {
    LOGE("wifi", "down");
  }
  wasConnected = now;

  if (now) {
    return;
  }

  static uint32_t lastTry = 0;
  if (millis() - lastTry > WIFI_RETRY_MS) {
    lastTry = millis();
    LOGI("wifi", "reconnect()");
    WiFi.reconnect();
  }
}
