#pragma once

#include <Arduino.h>

struct CatalogObject {
  String name;
  String type;
  double raHours;
  double decDegrees;
  double magnitude;
};

namespace catalog {

bool init();
size_t size();
const CatalogObject* get(size_t index);
const CatalogObject* findByName(const String& name);

}  // namespace catalog

