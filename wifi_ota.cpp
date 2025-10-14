#include "wifi_ota.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "config.h"
#include "storage.h"
#include "time_utils.h"

#if defined(DEVICE_ROLE_HID)
#include "display_menu.h"
#endif

namespace wifi_ota {
namespace {

bool wifiEnabled = false;
bool wifiConnected = false;
bool otaActive = false;
bool ntpSynced = false;
String otaHostname;

constexpr uint32_t kReconnectIntervalMs = 10000;
constexpr uint32_t kNtpResyncIntervalMs = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kNtpRetryIntervalMs = 60000;

uint32_t lastReconnectAttemptMs = 0;
uint32_t lastNtpSyncMs = 0;
uint32_t lastNtpAttemptMs = 0;

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
  if (otaHostname.isEmpty()) {
    otaHostname = String(config::WIFI_HOSTNAME_PREFIX) + "-" + roleSuffix();
  }
}

void stopWifi() {
  if (otaActive) {
    ArduinoOTA.end();
    otaActive = false;
  }
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  wifiEnabled = false;
  wifiConnected = false;
  ntpSynced = false;
  lastReconnectAttemptMs = 0;
  lastNtpSyncMs = 0;
  lastNtpAttemptMs = 0;
}

bool credentialsAvailable() { return storage::hasWifiCredentials(); }

void beginWifi() {
  ensureIdentity();
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect(true, true);
  WiFi.setHostname(otaHostname.c_str());
  WiFi.setAutoReconnect(true);
  ArduinoOTA.setHostname(otaHostname.c_str());
  WiFi.begin(storage::wifiSsid(), storage::wifiPassword());
  wifiEnabled = true;
  wifiConnected = false;
  otaActive = false;
  ntpSynced = false;
  lastReconnectAttemptMs = millis();
  lastNtpSyncMs = 0;
  lastNtpAttemptMs = 0;
}

bool syncTimeWithNtp() {
  static const char* servers[] = {"pool.ntp.org", "time.nist.gov", "time.cloudflare.com"};
  configTime(0, 0, servers[0], servers[1], servers[2]);
  struct tm timeinfo{};
  if (!getLocalTime(&timeinfo, 10000)) {
    return false;
  }
  time_t utcEpoch = mktime(&timeinfo);
  DateTime localTime = time_utils::applyTimezone(utcEpoch);
#if defined(DEVICE_ROLE_HID)
  display_menu::applyNetworkTime(localTime);
#else
  storage::setRtcEpoch(localTime.unixtime());
#endif
  return true;
}

void handleConnectedState() {
  if (!wifiConnected) {
    wifiConnected = true;
    if (!otaActive) {
      ArduinoOTA.begin();
      otaActive = true;
    }
    ntpSynced = false;
    lastNtpSyncMs = 0;
    lastNtpAttemptMs = 0;
  }

  if (otaActive) {
    ArduinoOTA.handle();
  }

  uint32_t now = millis();
  bool shouldSync = false;
  if (!ntpSynced) {
    if (now - lastNtpAttemptMs >= kNtpRetryIntervalMs) {
      shouldSync = true;
    }
  } else if (now - lastNtpSyncMs >= kNtpResyncIntervalMs &&
             now - lastNtpAttemptMs >= kNtpRetryIntervalMs) {
    shouldSync = true;
  }

  if (shouldSync) {
    if (syncTimeWithNtp()) {
      ntpSynced = true;
      lastNtpSyncMs = now;
    }
    lastNtpAttemptMs = now;
  }
}

void handleDisconnectedState() {
  if (wifiConnected || otaActive) {
    if (otaActive) {
      ArduinoOTA.end();
      otaActive = false;
    }
    wifiConnected = false;
  }
  uint32_t now = millis();
  if (wifiEnabled && credentialsAvailable() &&
      (lastReconnectAttemptMs == 0 || now - lastReconnectAttemptMs >= kReconnectIntervalMs)) {
    WiFi.disconnect(true, true);
    WiFi.begin(storage::wifiSsid(), storage::wifiPassword());
    lastReconnectAttemptMs = now;
  }
}

}  // namespace

void init() {
  ensureIdentity();
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);
  wifiEnabled = false;
  wifiConnected = false;
  otaActive = false;
  ntpSynced = false;
  lastReconnectAttemptMs = 0;
  lastNtpSyncMs = 0;
  lastNtpAttemptMs = 0;
}

void setEnabled(bool enabled) {
  if (!enabled) {
    if (wifiEnabled || wifiConnected) {
      stopWifi();
    }
    return;
  }

  if (wifiEnabled) {
    return;
  }

  if (!credentialsAvailable()) {
    wifiEnabled = false;
    return;
  }

  beginWifi();
}

bool isEnabled() { return wifiEnabled; }

const char* hostname() { return otaHostname.c_str(); }

bool credentialsConfigured() { return credentialsAvailable(); }

bool isConnected() { return wifiConnected && WiFi.status() == WL_CONNECTED; }

const char* ssid() { return storage::wifiSsid(); }

void update() {
  if (!wifiEnabled) {
    return;
  }

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    handleConnectedState();
  } else {
    handleDisconnectedState();
  }
}

}  // namespace wifi_ota

