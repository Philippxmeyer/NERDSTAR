#include "text_utils.h"

String sanitizeForDisplay(const String& text) {
  String result;
  result.reserve(text.length() + 8);
  const char* raw = text.c_str();
  size_t len = text.length();
  for (size_t i = 0; i < len;) {
    uint8_t c = static_cast<uint8_t>(raw[i]);
    if (c == static_cast<uint8_t>(kDegreeSymbol)) {
      result += kDegreeSymbol;
      ++i;
      continue;
    }
    if (c < 0x80) {
      if (c == 0xB0) {
        result += kDegreeSymbol;
      } else {
        result += static_cast<char>(c);
      }
      ++i;
      continue;
    }
    if (c == 0xC3 && i + 1 < len) {
      uint8_t next = static_cast<uint8_t>(raw[i + 1]);
      switch (next) {
        case 0xA4:  // ä
          result += "ae";
          break;
        case 0xB6:  // ö
          result += "oe";
          break;
        case 0xBC:  // ü
          result += "ue";
          break;
        case 0x84:  // Ä
          result += "Ae";
          break;
        case 0x96:  // Ö
          result += "Oe";
          break;
        case 0x9C:  // Ü
          result += "Ue";
          break;
        case 0x9F:  // ß
          result += "ss";
          break;
        case 0xA9:  // é
          result += 'e';
          break;
        default:
          result += '?';
          break;
      }
      i += 2;
      continue;
    }
    if (c == 0xC2 && i + 1 < len) {
      uint8_t next = static_cast<uint8_t>(raw[i + 1]);
      if (next == 0xB0) {  // °
        result += kDegreeSymbol;
      } else if (next == 0xB5) {  // µ
        result += 'u';
      } else {
        result += '?';
      }
      i += 2;
      continue;
    }
    // Unsupported sequences are replaced with a placeholder to avoid
    // breaking text layout.
    result += '?';
    ++i;
  }
  return result;
}

