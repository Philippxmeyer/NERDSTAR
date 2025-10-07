#include "planets.h"

#include <math.h>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kRadToHour = 12.0 / kPi;

struct OrbitalElements {
  double a0;
  double a1;
  double e0;
  double e1;
  double i0;
  double i1;
  double L0;
  double L1;
  double peri0;
  double peri1;
  double node0;
  double node1;
};

const OrbitalElements kElements[] = {
    // Mercury
    {0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.25032350,
     149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081},
    // Venus
    {0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890, 181.97909950,
     58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418},
    // Earth
    {1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166,
     35999.37244981, 102.93768193, 0.32327364, 0.0, 0.0},
    // Mars
    {1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131, -4.55343205,
     19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343},
    // Jupiter
    {5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051,
     3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106},
    // Saturn
    {9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423,
     1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794},
    // Uranus
    {19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939, 313.23810451,
     428.48202785, 170.95427630, 0.40805281, 74.01692503, 0.04240589},
    // Neptune
    {30.06992276, 0.00026291, 0.00859048, 0.00005105, 1.77004347, 0.00035372, -55.12002969,
     218.45945325, 44.96476227, -0.32241464, 131.78422574, -0.00508664},
};

struct Vec3 {
  double x;
  double y;
  double z;
};

double normalizeAngle(double value) {
  value = fmod(value, 360.0);
  if (value < 0) value += 360.0;
  return value;
}

double normalizeRadians(double value) {
  value = fmod(value, 2 * kPi);
  if (value < 0) value += 2 * kPi;
  return value;
}

double solveKepler(double M, double e) {
  double E = M;
  for (int i = 0; i < 5; ++i) {
    double delta = (E - e * sin(E) - M) / (1.0 - e * cos(E));
    E -= delta;
    if (fabs(delta) < 1e-8) {
      break;
    }
  }
  return E;
}

Vec3 heliocentricEcliptic(PlanetId id, double T) {
  const OrbitalElements& el = kElements[static_cast<int>(id)];
  double a = el.a0 + el.a1 * T;
  double e = el.e0 + el.e1 * T;
  double I = (el.i0 + el.i1 * T) * kDegToRad;
  double L = normalizeAngle(el.L0 + el.L1 * T) * kDegToRad;
  double peri = normalizeAngle(el.peri0 + el.peri1 * T) * kDegToRad;
  double node = normalizeAngle(el.node0 + el.node1 * T) * kDegToRad;
  double M = normalizeRadians(L - peri);
  double E = solveKepler(M, e);
  double xv = cos(E) - e;
  double yv = sqrt(1 - e * e) * sin(E);
  double v = atan2(yv, xv);
  double r = a * (1 - e * cos(E));
  double w = peri - node;
  double cosO = cos(node);
  double sinO = sin(node);
  double cosI = cos(I);
  double sinI = sin(I);
  double cosvw = cos(v + w);
  double sinvw = sin(v + w);

  Vec3 result;
  result.x = r * (cosO * cosvw - sinO * sinvw * cosI);
  result.y = r * (sinO * cosvw + cosO * sinvw * cosI);
  result.z = r * (sinvw * sinI);
  return result;
}

PlanetPosition computeGeocentric(PlanetId planet, double T) {
  Vec3 planetVec = heliocentricEcliptic(planet, T);
  Vec3 earthVec = heliocentricEcliptic(PlanetId::Earth, T);
  Vec3 geo{planetVec.x - earthVec.x, planetVec.y - earthVec.y, planetVec.z - earthVec.z};

  double epsilon = (23.439291 - 0.0130042 * T) * kDegToRad;
  double cosEps = cos(epsilon);
  double sinEps = sin(epsilon);
  double x = geo.x;
  double y = geo.y * cosEps - geo.z * sinEps;
  double z = geo.y * sinEps + geo.z * cosEps;

  double ra = atan2(y, x);
  if (ra < 0) {
    ra += 2 * kPi;
  }
  double dec = atan2(z, sqrt(x * x + y * y));
  double distance = sqrt(x * x + y * y + z * z);

  PlanetPosition position{normalizeRadians(ra) * kRadToHour, dec * kRadToDeg, distance};
  if (position.raHours < 0) {
    position.raHours += 24.0;
  }
  return position;
}

}  // namespace

namespace planets {

double julianDay(int year, int month, int day, double hourFraction) {
  if (month <= 2) {
    year -= 1;
    month += 12;
  }
  int A = year / 100;
  int B = 2 - A + A / 4;
  double JD = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
  JD += hourFraction / 24.0;
  return JD;
}

bool computePlanet(PlanetId id, double julianDay, PlanetPosition& out) {
  if (id == PlanetId::Earth) {
    return false;
  }
  double T = (julianDay - 2451545.0) / 36525.0;
  out = computeGeocentric(id, T);
  return true;
}

bool planetFromString(const String& name, PlanetId& id) {
  String lower = name;
  lower.toLowerCase();
  if (lower == "mercury") {
    id = PlanetId::Mercury;
    return true;
  }
  if (lower == "venus") {
    id = PlanetId::Venus;
    return true;
  }
  if (lower == "earth" || lower == "earth moon" || lower == "moon") {
    id = PlanetId::Earth;
    return false;
  }
  if (lower == "mars") {
    id = PlanetId::Mars;
    return true;
  }
  if (lower == "jupiter") {
    id = PlanetId::Jupiter;
    return true;
  }
  if (lower == "saturn") {
    id = PlanetId::Saturn;
    return true;
  }
  if (lower == "uranus") {
    id = PlanetId::Uranus;
    return true;
  }
  if (lower == "neptune") {
    id = PlanetId::Neptune;
    return true;
  }
  return false;
}

}  // namespace planets

