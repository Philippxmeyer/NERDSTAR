#pragma once

#include <Arduino.h>

struct PlanetPosition {
  double raHours;
  double decDegrees;
  double distanceAu;
};

enum class PlanetId {
  Mercury,
  Venus,
  Earth,
  Mars,
  Jupiter,
  Saturn,
  Uranus,
  Neptune,
};

namespace planets {

bool computePlanet(PlanetId id, double julianDay, PlanetPosition& out);
bool planetFromString(const String& name, PlanetId& id);
double julianDay(int year, int month, int day, double hourFraction);

}  // namespace planets

