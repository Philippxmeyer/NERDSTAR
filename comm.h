#pragma once

#include <Arduino.h>

#include <initializer_list>
#include <vector>

#include "config.h"

namespace comm {

struct Request {
  uint16_t id;
  String command;
  std::vector<String> params;
};

void initLink();

#if defined(DEVICE_ROLE_HID)
bool waitForReady(uint32_t timeoutMs);
bool call(const char* command, std::initializer_list<String> params,
          std::vector<String>* payload = nullptr, String* error = nullptr,
          uint32_t timeoutMs = config::COMM_RESPONSE_TIMEOUT_MS);
#elif defined(DEVICE_ROLE_MAIN)
void announceReady();
bool readRequest(Request& request,
                 uint32_t timeoutMs = config::COMM_RESPONSE_TIMEOUT_MS);
void sendOk(uint16_t id, std::initializer_list<String> payload = {});
void sendError(uint16_t id, const String& message);
#endif

}  // namespace comm

