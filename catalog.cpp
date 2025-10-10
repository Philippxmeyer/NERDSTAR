#include "catalog.h"

#include <vector>

#include "storage.h"
#include "text_utils.h"

namespace {

std::vector<CatalogObject> catalogObjects;

constexpr const char* kTypeNames[] = {
    "Planet", "Moon", "Star", "Cluster", "Double Star", "Galaxy", "Nebula", "Planetary Nebula"};

const char* resolveTypeName(uint8_t index) {
  if (index >= sizeof(kTypeNames) / sizeof(kTypeNames[0])) {
    return "Unknown";
  }
  return kTypeNames[index];
}

}  // namespace

namespace catalog {

bool init() {
  catalogObjects.clear();
  size_t count = storage::getCatalogEntryCount();
  catalogObjects.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    storage::CatalogEntry entry{};
    if (!storage::readCatalogEntry(i, entry)) {
      continue;
    }
    char nameBuffer[32];
    if (!storage::readCatalogName(entry.nameOffset, entry.nameLength, nameBuffer, sizeof(nameBuffer))) {
      continue;
    }

    CatalogObject object;
    object.name = sanitizeForDisplay(String(nameBuffer));
    object.type = sanitizeForDisplay(String(resolveTypeName(entry.typeIndex)));
    object.raHours = static_cast<double>(entry.raHoursTimes1000) / 1000.0;
    object.decDegrees = static_cast<double>(entry.decDegreesTimes100) / 100.0;
    object.magnitude = static_cast<double>(entry.magnitudeTimes10) / 10.0;
    catalogObjects.push_back(std::move(object));
  }
  return !catalogObjects.empty();
}

size_t size() { return catalogObjects.size(); }

const CatalogObject* get(size_t index) {
  if (index >= catalogObjects.size()) {
    return nullptr;
  }
  return &catalogObjects[index];
}

const CatalogObject* findByName(const String& name) {
  String sanitized = sanitizeForDisplay(name);
  for (auto& obj : catalogObjects) {
    if (obj.name.equalsIgnoreCase(sanitized)) {
      return &obj;
    }
  }
  return nullptr;
}

}  // namespace catalog
