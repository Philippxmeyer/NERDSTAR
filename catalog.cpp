#include "catalog.h"

#include <algorithm>
#include <array>
#include <vector>

#include "storage.h"
#include "text_utils.h"

namespace {

std::vector<CatalogObject> catalogObjects;

constexpr const char* kTypeNames[] = {
    "Planet", "Moon", "Star", "Cluster", "Double Star", "Galaxy", "Nebula", "Planetary Nebula"};

constexpr size_t kTypeCount = sizeof(kTypeNames) / sizeof(kTypeNames[0]);
std::array<std::vector<size_t>, kTypeCount> typeBuckets;
std::array<String, kTypeCount> sanitizedTypeNames;
std::vector<uint8_t> activeTypes;

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
  activeTypes.clear();
  for (size_t i = 0; i < kTypeCount; ++i) {
    typeBuckets[i].clear();
    sanitizedTypeNames[i] = sanitizeForDisplay(String(resolveTypeName(static_cast<uint8_t>(i))));
  }
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
    object.typeIndex = entry.typeIndex;
    if (entry.typeIndex < kTypeCount) {
      object.type = sanitizedTypeNames[entry.typeIndex];
    } else {
      object.type = sanitizeForDisplay(String(resolveTypeName(entry.typeIndex)));
    }
    object.raHours = static_cast<double>(entry.raHoursTimes1000) / 1000.0;
    object.decDegrees = static_cast<double>(entry.decDegreesTimes100) / 100.0;
    object.magnitude = static_cast<double>(entry.magnitudeTimes10) / 10.0;
    catalogObjects.push_back(std::move(object));
    size_t objectIndex = catalogObjects.size() - 1;
    if (entry.typeIndex < kTypeCount) {
      typeBuckets[entry.typeIndex].push_back(objectIndex);
    }
  }
  for (uint8_t i = 0; i < kTypeCount; ++i) {
    if (!typeBuckets[i].empty()) {
      activeTypes.push_back(i);
    }
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

size_t typeGroupCount() { return activeTypes.size(); }

bool getTypeSummary(size_t index, CatalogTypeSummary& summary) {
  if (index >= activeTypes.size()) {
    return false;
  }
  uint8_t typeIndex = activeTypes[index];
  summary.typeIndex = typeIndex;
  summary.name = sanitizedTypeNames[typeIndex];
  summary.objectCount = typeBuckets[typeIndex].size();
  return true;
}

bool getTypeObjectIndex(size_t typeGroupIndex, size_t objectOffset, size_t& outIndex) {
  if (typeGroupIndex >= activeTypes.size()) {
    return false;
  }
  uint8_t typeIndex = activeTypes[typeGroupIndex];
  const auto& bucket = typeBuckets[typeIndex];
  if (objectOffset >= bucket.size()) {
    return false;
  }
  outIndex = bucket[objectOffset];
  return true;
}

int findTypeGroupForObject(size_t catalogIndex) {
  if (catalogIndex >= catalogObjects.size()) {
    return -1;
  }
  for (size_t i = 0; i < activeTypes.size(); ++i) {
    uint8_t typeIndex = activeTypes[i];
    const auto& bucket = typeBuckets[typeIndex];
    if (std::find(bucket.begin(), bucket.end(), catalogIndex) != bucket.end()) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int findTypeLocalIndex(size_t typeGroupIndex, size_t catalogIndex) {
  if (typeGroupIndex >= activeTypes.size()) {
    return -1;
  }
  uint8_t typeIndex = activeTypes[typeGroupIndex];
  const auto& bucket = typeBuckets[typeIndex];
  for (size_t i = 0; i < bucket.size(); ++i) {
    if (bucket[i] == catalogIndex) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace catalog
