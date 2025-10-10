#pragma once

#include <Arduino.h>

inline constexpr char kDegreeSymbol = static_cast<char>(248);

String sanitizeForDisplay(const String& text);

