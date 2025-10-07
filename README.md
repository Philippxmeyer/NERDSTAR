# 🤓✨ NERDSTAR

### Navigation, Elevation & Rotation Drive System for Telescope Alt/Az Regulation

> *Because even the cosmos deserves a little nerd power.*

---

## 🚀 Was ist NERDSTAR?

NERDSTAR ist der Versuch, einen ESP32 dazu zu überreden,
dein Teleskop mit der Präzision eines NASA-Gyros und dem Charme eines Bastelkellers zu bewegen.

Zwei **TMC2209**-Treiber, ein **Joystick**, ein **OLED**, eine **RTC**
und ein Hauch Größenwahn ergeben zusammen ein
**Alt/Az-Steuerungssystem mit µs-Timing und Stil**.

> Kein KI-Overkill.
> Nur ehrlicher Schrittmotor-Schweiß und ein bisschen Mathematik.

---

## 🧬 Features

| Kategorie                     | Beschreibung                                                           |
| ----------------------------- | ---------------------------------------------------------------------- |
| 🔭 **Dual Axis Control**      | Zwei Achsen (Azimuth & Altitude) mit unabhängigen Hardware-Timern      |
| 🔹 **Joystick Navigation**    | X = Azimut, Y = Höhe, Taste = Pause, Spaßfaktor = Hoch                 |
| 🕒 **RTC DS3231**             | Präzise Zeitsynchronisierung (später wichtig für Tracking)             |
| 📺 **OLED Status Display**    | Zeigt Live-RPM, Zeit, Systemstatus                                     |
| ⚙️ **µs-Timersteuerung**      | Stepper laufen so gleichmäßig, dass man sie fast atmen hört            |
| 🧠 **ESP32 Dual-Core**        | Core 1 = Stepper / Core 0 = Display – keine Ruckler, keine Kompromisse |
| 🧮 **Alt/Az Framework Ready** | Platz für Astronomie-Formeln, trigonometrische Eskapaden & Sternzeit   |

---

## 💮 Projektziel (philosophisch betrachtet)

> „Wenn’s sich dreht und der Himmel nicht wegläuft, war’s ein Erfolg.“
> — *Philipp, 2025*

NERDSTAR ist modular aufgebaut:
Erst manuell bewegen, dann automatisch nachführen,
und irgendwann sagen: „Lauf, kleiner ESP, lauf mit den Sternen.“

---

## ⚙️ Hardwareübersicht

| Komponente             | Aufgabe                             | Pins                                  |
| ---------------------- | ----------------------------------- | ------------------------------------- |
| **ESP32**              | Gehirn                              | –                                     |
| **TMC2209 (Azimut)**   | Dreht nach links/rechts             | STEP 25, DIR 26, EN 27, RX/TX = 16/17 |
| **TMC2209 (Altitude)** | Dreht nach oben/unten               | STEP 13, DIR 12, EN 14, RX/TX = 4/5   |
| **OLED (SSD1306)**     | Zeigt alles an, außer Mitleid       | I²C: SDA 21, SCL 22                   |
| **RTC (DS3231)**       | Sagt dir, wann du’s verpasst hast   | I²C: SDA 21, SCL 22                   |
| **Joystick (KY-023)**  | Steuert alles intuitiv falsch herum | VRx 34, VRy 35, SW 32                 |

---

## 🧠 Softwarestruktur

```
NERDSTAR/
│
├── NERDSTAR.ino          # Der eigentliche Wahnsinn
├── README.md              # Dieses Manifest
├── LICENSE                # Vermutlich MIT, weil wir nett sind
└── docs/
    ├── wiring-diagram.png # Irgendwann mal
    └── hardware-notes.md  # Vielleicht
```

---

## 📺 OLED-Anzeige

```
NERDSTAR v1.0
AZ: +1.23 RPM
ALT: -0.87 RPM
Time: 21:47:13
```

> Wenn du das siehst, läuft’s.
> Wenn nicht, läuft’s vermutlich auch – nur in die falsche Richtung.

---

## �� Tracking demnächst™

NERDSTAR ist schon bereit für die große Bühne:

```cpp
void computeTrackingRates(double latitude, double longitude, DateTime now) {
  // TODO:
  // 1. RA/DEC -> Alt/Az umrechnen
  // 2. Sternzeit bestimmen
  // 3. Schrittfrequenzen anpassen
  // 4. Kaffee holen
}
```

Ziel: das Teleskop bewegt sich exakt so,
dass die Sterne stillstehen – oder wenigstens so tun.

---

## 🧰 Abhängigkeiten

| Bibliothek         | Zweck                     | Empfohlene Version |
| ------------------ | ------------------------- | ------------------ |
| `TMCStepper`       | Kommunikation mit TMC2209 | ≥ 0.7.3            |
| `Adafruit_SSD1306` | OLED-Anzeige              | ≥ 2.5.9            |
| `Adafruit_GFX`     | Grafik-Backend            | ≥ 1.11.9           |
| `RTClib`           | DS3231 RTC                | ≥ 2.1.3            |

---

## ⚡ Installation

1. Arduino IDE öffnen
2. Board: **ESP32 Dev Module**
3. Bibliotheken installieren (siehe oben)
4. `NERDSTAR.ino` hochladen
5. Kaffee holen
6. Freuen, dass du was gebaut hast, das klingt wie ein NASA-Projekt und aussieht wie ein Nerd-Traum.

---

## 📸 Fun Fact

> „Warum NERDSTAR?“
> – Weil *‘Alt/Az-Mount Controller with Dual-Axis Timer Sync and Real-Time RTC Support’*
> nicht gut auf ein T-Shirt passt.

---

## 💩 Lizenz

MIT License © 2025 Philipp

> Benutze, forke, modifiziere – aber denk dran:
> Auch im Weltall gilt: Kein Support bei falscher Polausrichtung.

---

## ✨ Schlusswort

**NERDSTAR** –
Ein Projekt für Menschen, die beim Wort *Stepper*
nicht an Aerobic, sondern an Himmelsmechanik denken.

🤓🌠 *Stay nerdy, track steadily.*
