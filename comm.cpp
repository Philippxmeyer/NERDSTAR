#include "comm.h"

#include <algorithm>

namespace {

HardwareSerial& link = Serial;
uint16_t nextRequestId = 1;

bool readLine(String& line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  while (true) {
    while (link.available()) {
      char c = static_cast<char>(link.read());
      if (c == '\n') {
        return true;
      }
      if (c != '\r') {
        line += c;
      }
    }
    if (timeoutMs != 0 && (millis() - start) >= timeoutMs) {
      return false;
    }
    delay(1);
  }
}

void sendLine(const String& line) {
  link.print(line);
  link.print('\n');
}

void splitFields(const String& line, std::vector<String>& fields) {
  fields.clear();
  int start = 0;
  while (start <= line.length()) {
    int idx = line.indexOf('|', start);
    if (idx < 0) {
      fields.push_back(line.substring(start));
      break;
    }
    fields.push_back(line.substring(start, idx));
    start = idx + 1;
  }
}

}  // namespace

namespace comm {

void initLink() {
  link.begin(config::COMM_BAUDRATE, SERIAL_8N1, config::COMM_RX_PIN,
             config::COMM_TX_PIN);
  link.flush();
}

#if defined(DEVICE_ROLE_HID)

bool waitForReady(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (true) {
    uint32_t remaining = 0;
    if (timeoutMs != 0) {
      uint32_t elapsed = millis() - start;
      if (elapsed >= timeoutMs) {
        return false;
      }
      remaining = timeoutMs - elapsed;
    }
    String line;
    if (!readLine(line, remaining)) {
      return false;
    }
    if (line == "READY") {
      return true;
    }
  }
}

bool call(const char* command, std::initializer_list<String> params,
          std::vector<String>* payload, String* error, uint32_t timeoutMs) {
  if (payload) {
    payload->clear();
  }
  uint16_t id = nextRequestId++;
  String line = "REQ|" + String(id) + "|" + String(command);
  for (const auto& param : params) {
    line += "|";
    line += param;
  }
  sendLine(line);

  uint32_t start = millis();
  while (true) {
    uint32_t remaining = 0;
    if (timeoutMs != 0) {
      uint32_t elapsed = millis() - start;
      if (elapsed >= timeoutMs) {
        if (error) {
          *error = "Timeout";
        }
        return false;
      }
      remaining = timeoutMs - elapsed;
    }
    String response;
    if (!readLine(response, remaining)) {
      if (error) {
        *error = "Timeout";
      }
      return false;
    }
    if (response == "READY") {
      continue;
    }
    std::vector<String> fields;
    splitFields(response, fields);
    if (fields.empty()) {
      continue;
    }
    if (fields[0] != "RESP") {
      continue;
    }
    if (fields.size() < 3) {
      continue;
    }
    uint16_t respId = static_cast<uint16_t>(fields[1].toInt());
    if (respId != id) {
      continue;
    }
    const String& status = fields[2];
    if (status == "OK") {
      if (payload) {
        payload->assign(fields.begin() + 3, fields.end());
      }
      return true;
    }
    if (error) {
      if (fields.size() > 3) {
        *error = fields[3];
      } else {
        *error = "Error";
      }
    }
    return false;
  }
}

#elif defined(DEVICE_ROLE_MAIN)

void announceReady() { sendLine("READY"); }

bool readRequest(Request& request, uint32_t timeoutMs) {
  while (true) {
    String line;
    if (!readLine(line, timeoutMs)) {
      return false;
    }
    if (line == "READY") {
      continue;
    }
    std::vector<String> fields;
    splitFields(line, fields);
    if (fields.empty()) {
      continue;
    }
    if (fields[0] != "REQ") {
      continue;
    }
    if (fields.size() < 3) {
      continue;
    }
    request.id = static_cast<uint16_t>(fields[1].toInt());
    request.command = fields[2];
    request.params.assign(fields.begin() + 3, fields.end());
    return true;
  }
}

void sendOk(uint16_t id, std::initializer_list<String> payload) {
  String line = "RESP|" + String(id) + "|OK";
  for (const auto& value : payload) {
    line += "|";
    line += value;
  }
  sendLine(line);
}

void sendError(uint16_t id, const String& message) {
  String line = "RESP|" + String(id) + "|ERR|" + message;
  sendLine(line);
}

#endif

}  // namespace comm

