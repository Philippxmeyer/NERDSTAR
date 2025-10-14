#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <time.h>

#include "storage.h"

namespace time_utils {

time_t toUtcEpoch(const DateTime& localTime);
DateTime applyTimezone(time_t utcEpoch);
bool isDstActiveLocal(const DateTime& localTime);
}

