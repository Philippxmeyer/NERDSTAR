#include "time_utils.h"

namespace {

constexpr int kDstOffsetMinutes = 60;

int lastSundayOfMonth(int year, int month) {
  DateTime lastDay(year, month, 31, 0, 0, 0);
  int weekday = lastDay.dayOfTheWeek();  // 0 = Sunday
  return 31 - weekday;
}

bool europeDstActive(time_t utcEpoch, int32_t timezoneMinutes) {
  DateTime utc(utcEpoch);
  int year = utc.year();

  int marchSunday = lastSundayOfMonth(year, 3);
  DateTime dstStartLocal(year, 3, marchSunday, 2, 0, 0);
  time_t dstStartUtc = dstStartLocal.unixtime() - static_cast<time_t>(timezoneMinutes) * 60;

  int octoberSunday = lastSundayOfMonth(year, 10);
  DateTime dstEndLocal(year, 10, octoberSunday, 3, 0, 0);
  time_t dstEndUtc =
      dstEndLocal.unixtime() - static_cast<time_t>(timezoneMinutes + kDstOffsetMinutes) * 60;

  if (utcEpoch < dstStartUtc) {
    return false;
  }
  if (utcEpoch >= dstEndUtc) {
    return false;
  }
  return true;
}

int32_t dstOffsetFromUtc(time_t utcEpoch, storage::DstMode mode, int32_t timezoneMinutes) {
  switch (mode) {
    case storage::DstMode::Off:
      return 0;
    case storage::DstMode::On:
      return kDstOffsetMinutes;
    case storage::DstMode::Auto:
      return europeDstActive(utcEpoch, timezoneMinutes) ? kDstOffsetMinutes : 0;
  }
  return 0;
}

}  // namespace

namespace time_utils {

time_t toUtcEpoch(const DateTime& localTime) {
  const SystemConfig& config = storage::getConfig();
  time_t utcEpoch = localTime.unixtime();
  utcEpoch -= static_cast<time_t>(config.timezoneOffsetMinutes) * 60;
  int32_t dstMinutes = dstOffsetFromUtc(utcEpoch, config.dstMode, config.timezoneOffsetMinutes);
  utcEpoch -= static_cast<time_t>(dstMinutes) * 60;
  return utcEpoch;
}

DateTime applyTimezone(time_t utcEpoch) {
  const SystemConfig& config = storage::getConfig();
  time_t localEpoch = utcEpoch + static_cast<time_t>(config.timezoneOffsetMinutes) * 60;
  int32_t dstMinutes = dstOffsetFromUtc(utcEpoch, config.dstMode, config.timezoneOffsetMinutes);
  localEpoch += static_cast<time_t>(dstMinutes) * 60;
  return DateTime(localEpoch);
}

bool isDstActiveLocal(const DateTime& localTime) {
  const SystemConfig& config = storage::getConfig();
  time_t utcEpoch = toUtcEpoch(localTime);
  return dstOffsetFromUtc(utcEpoch, config.dstMode, config.timezoneOffsetMinutes) > 0;
}

}  // namespace time_utils

