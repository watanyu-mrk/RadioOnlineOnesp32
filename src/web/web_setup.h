#pragma once

// Access-point setup portal: Wi-Fi credentials + station list, edited from a phone.
//
// Entered from the SETUP button or on first boot when no Wi-Fi is stored. The player stops
// streaming while the portal runs and reboots when the page asks for it.
//
// AP_STA (not plain AP): scanning for networks while the portal is up needs the STA
// interface, and in AP-only mode a scan can drop the phone that is editing the page.

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "radio/station_manager.h"

#define SETUP_AP_SSID "RetroRadio-Setup"
#define SETUP_AP_PASS "radio1234"

class WebSetup {
 public:
  void begin(StationManager *stations);
  void loop();

  bool running() const { return running_; }
  String ip() const { return WiFi.softAPIP().toString(); }

 private:
  static String jsonEscape(const char *s);
  void routes();

  WebServer server_{80};
  StationManager *stations_ = nullptr;
  bool running_ = false;
  bool staStarted_ = false;
  bool staReported_ = false;
  uint32_t staSince_ = 0;
  uint32_t rebootAt_ = 0;

  static const char PAGE[] PROGMEM;
};
