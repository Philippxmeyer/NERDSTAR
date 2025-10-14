#pragma once

#include <Arduino.h>

struct CatalogObject {
  String name;
  String code;
  String type;
  uint8_t typeIndex;
  double raHours;
  double decDegrees;
  double magnitude;
};

struct CatalogTypeSummary {
  uint8_t typeIndex;
  String name;
  size_t objectCount;
};

namespace catalog {

bool init();
size_t size();
const CatalogObject* get(size_t index);
const CatalogObject* findByName(const String& name);
size_t typeGroupCount();
bool getTypeSummary(size_t index, CatalogTypeSummary& summary);
bool getTypeObjectIndex(size_t typeGroupIndex, size_t objectOffset, size_t& outIndex);
int findTypeGroupForObject(size_t catalogIndex);
int findTypeLocalIndex(size_t typeGroupIndex, size_t catalogIndex);

}  // namespace catalog

