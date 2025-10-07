#include "catalog.h"

#include <SD.h>
#include <algorithm>
#include <vector>

#include "catalog_data.h"

namespace {

std::vector<CatalogObject> catalogObjects;
bool loaded = false;

String readLine(File& file) {
  return file.readStringUntil('\n');
}

String readAttribute(const String& element, const char* attribute) {
  String pattern = String(attribute) + "=\"";
  int start = element.indexOf(pattern);
  if (start < 0) {
    return String();
  }
  start += pattern.length();
  int end = element.indexOf('"', start);
  if (end < 0) {
    return String();
  }
  return element.substring(start, end);
}

bool parseObjectElement(const String& line, CatalogObject& object) {
  if (!line.startsWith("<object") || !line.endsWith("/>")) {
    return false;
  }

  String name = readAttribute(line, "name");
  String type = readAttribute(line, "type");
  String ra = readAttribute(line, "ra_hours");
  String dec = readAttribute(line, "dec_degrees");
  String mag = readAttribute(line, "magnitude");

  if (name.isEmpty() || type.isEmpty() || ra.isEmpty() || dec.isEmpty() || mag.isEmpty()) {
    return false;
  }

  object.name = name;
  object.type = type;
  object.raHours = ra.toFloat();
  object.decDegrees = dec.toFloat();
  object.magnitude = mag.toFloat();
  return true;
}

bool ensureCatalogOnSd() {
  if (SD.exists("/catalog.xml")) {
    return true;
  }
  File file = SD.open("/catalog.xml", FILE_WRITE);
  if (!file) {
    return false;
  }
  size_t length = strlen_P(kDefaultCatalogXml);
  constexpr size_t kChunk = 256;
  char buffer[kChunk];
  size_t offset = 0;
  while (offset < length) {
    size_t toCopy = std::min(kChunk, length - offset);
    for (size_t i = 0; i < toCopy; ++i) {
      buffer[i] = pgm_read_byte_near(kDefaultCatalogXml + offset + i);
    }
    file.write(reinterpret_cast<const uint8_t*>(buffer), toCopy);
    offset += toCopy;
  }
  file.close();
  return true;
}

bool loadCatalog() {
  if (!ensureCatalogOnSd()) {
    return false;
  }
  File file = SD.open("/catalog.xml", FILE_READ);
  if (!file) {
    return false;
  }

  catalogObjects.clear();
  while (file.available()) {
    String line = readLine(file);
    line.trim();
    if (line.isEmpty()) {
      continue;
    }
    if (!line.startsWith("<object")) {
      continue;
    }
    CatalogObject obj;
    if (parseObjectElement(line, obj)) {
      catalogObjects.push_back(obj);
    }
  }
  file.close();
  loaded = !catalogObjects.empty();
  return loaded;
}

}  // namespace

namespace catalog {

bool init() {
  loaded = loadCatalog();
  return loaded;
}

size_t size() { return catalogObjects.size(); }

const CatalogObject* get(size_t index) {
  if (index >= catalogObjects.size()) {
    return nullptr;
  }
  return &catalogObjects[index];
}

const CatalogObject* findByName(const String& name) {
  for (auto& obj : catalogObjects) {
    if (obj.name.equalsIgnoreCase(name)) {
      return &obj;
    }
  }
  return nullptr;
}

}  // namespace catalog

