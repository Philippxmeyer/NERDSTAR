#pragma once

#include <Arduino.h>

enum class MenuMode {
  Status,
  PolarAlign,
  Setup,
  Catalog,
  Goto
};

struct SystemState {
  MenuMode menuMode;
  bool polarAligned;
  bool trackingActive;
  bool sdAvailable;
  bool gotoActive;
  int selectedCatalogIndex;
  int64_t raGotoTarget;
  int64_t decGotoTarget;
};

extern SystemState systemState;

