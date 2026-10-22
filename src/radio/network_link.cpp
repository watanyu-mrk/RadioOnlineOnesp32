#include "radio/network_link.h"

#include <WiFi.h>
#include <time.h>

#include "app_config.h"

bool NetworkLink::connect(const char *ssid, const char *pass, uint32_t timeoutMs) {
  Serial.printf("wifi      : connecting to \"%s\"\n", ssid ? ssid : "");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid ? ssid : "", pass ? pass : "");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("wifi      : FAILED status=%d\n", (int)WiFi.status());
    return false;
  }

  Serial.printf("wifi      : connected ip=%s rssi=%d dBm\n", ipString(), WiFi.RSSI());
  if (!timeConfigured_) {
    timeConfigured_ = true;
    configTzTime(APP_TZ, APP_NTP_1, APP_NTP_2);
    Serial.println("time      : NTP requested (Asia/Bangkok, UTC+7)");
  }
  return true;
}

bool NetworkLink::connected() const { return WiFi.status() == WL_CONNECTED; }

int NetworkLink::rssi() const { return WiFi.RSSI(); }

IPAddress NetworkLink::ip() const { return WiFi.localIP(); }

const char *NetworkLink::ipString() {
  if (!connected()) return "-";
  snprintf(ip_, sizeof(ip_), "%s", WiFi.localIP().toString().c_str());
  return ip_;
}

bool NetworkLink::clock(uint8_t &hour, uint8_t &minute, uint8_t &second) {
  time_t now = time(nullptr);
  if (now < (time_t)APP_TIME_VALID_MIN) {  // NTP has not answered yet
    hour = minute = second = 0;
    return false;
  }
  struct tm tmv = {};
  localtime_r(&now, &tmv);
  hour = (uint8_t)tmv.tm_hour;
  minute = (uint8_t)tmv.tm_min;
  second = (uint8_t)tmv.tm_sec;
  return true;
}
