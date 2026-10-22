#pragma once

// Wi-Fi link + wall clock. Owns the STA connection, the IP string the panel shows and the
// NTP-synced Bangkok time, so the sketch never touches WiFi directly.

#include <Arduino.h>
#include <IPAddress.h>

class NetworkLink {
 public:
  // Blocking connect with a timeout (the board needs this before streaming). Starts NTP
  // once, on the first successful connection. Returns true when connected.
  bool connect(const char *ssid, const char *pass, uint32_t timeoutMs = 20000);

  bool connected() const;
  int rssi() const;
  IPAddress ip() const;
  // Cached dotted-quad, safe to hand to the UI.
  const char *ipString();

  // Bangkok local time. Returns false until NTP has set the clock.
  bool clock(uint8_t &hour, uint8_t &minute, uint8_t &second);

 private:
  char ip_[20] = "-";
  bool timeConfigured_ = false;
};
