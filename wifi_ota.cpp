#include "wifi_ota.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "config.h"

namespace wifi_ota {
namespace {

bool wifiEnabled = false;
String apSsid;
String otaHostname;

const char* roleSuffix() {
#if defined(DEVICE_ROLE_MAIN)
  return "MAIN";
#elif defined(DEVICE_ROLE_HID)
  return "HID";
#else
  return "DEV";
#endif
}

void ensureIdentity() {
  if (apSsid.isEmpty()) {
    apSsid = String(config::WIFI_AP_SSID_PREFIX) + "-" + roleSuffix();
  }
  if (otaHostname.isEmpty()) {
    otaHostname = String(config::WIFI_HOSTNAME_PREFIX) + "-" + roleSuffix();
  }
}

void stopWifi() {
  ArduinoOTA.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  wifiEnabled = false;
}

}  // namespace

void init() {
  ensureIdentity();
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);
  wifiEnabled = false;
}

void setEnabled(bool enabled) {
  if (enabled == wifiEnabled) {
    return;
  }

  if (!enabled) {
    stopWifi();
    return;
  }

  ensureIdentity();

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(apSsid.c_str(), config::WIFI_AP_PASSWORD);
  WiFi.softAPsetHostname(otaHostname.c_str());

  ArduinoOTA.setHostname(otaHostname.c_str());
  ArduinoOTA.begin();

  wifiEnabled = true;
}

bool isEnabled() { return wifiEnabled; }

const char* accessPointSsid() { return apSsid.c_str(); }

const char* hostname() { return otaHostname.c_str(); }

void update() {
  if (wifiEnabled) {
    ArduinoOTA.handle();
  }
}

}  // namespace wifi_ota

