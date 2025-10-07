![NERDSTAR Banner](docs/nerdstar-banner.png)


> *Because even the cosmos deserves a little nerd power.*

---

## 🚀 Was ist NERDSTAR?

NERDSTAR ist der Versuch, einen ESP32 dazu zu überreden,
dein Teleskop mit der Präzision eines NASA-Gyros und dem Charme eines Bastelkellers zu bewegen.

Zwei **TMC2209**-Treiber, ein **Joystick**, ein **OLED**, eine **RTC**
und ein Hauch Größenwahn ergeben zusammen ein
**RA/DEC-Steuerungssystem mit µs-Timing und Stil**.

> Kein KI-Overkill.
> Nur ehrlicher Schrittmotor-Schweiß und ein bisschen Mathematik.

---

## 🧬 Features

| Kategorie                     | Beschreibung                                                                 |
| ----------------------------- | ---------------------------------------------------------------------------- |
| 🔭 **Dual Axis Control**      | Zwei Achsen (RA & DEC) mit unabhängigen µs-Hardware-Timern                  |
| 🔹 **Joystick Navigation**    | Joystick für manuelles Slewen, Encoder für Menüs                            |
| 🧭 **Goto & Catalog**         | Objektbibliothek auf SD, Auswahl per Encoder, automatisches Goto             |
| 💾 **SD-Objektbibliothek**    | 200 vorkonfigurierte Sterne/Nebel/Galaxien/Planeten als CSV, automatisch geladen |
| 🕒 **RTC DS3231**             | Uhrzeit via Setup-Menü setzen, Grundlage für Planetenpositionen             |
| 🪐 **Planetenberechnung**     | Schlanker Algorithmus liefert aktuelle RA/Dec für klassische Planeten       |
| 🔧 **Setup & Kalibrierung**   | Menü für RTC-Zeit, Joystick-Zentrum, Achsenkalibrierung & EEPROM-Speicher   |
| 📺 **OLED Status Display**    | Zeigt RA/Dec, Tracking-/Goto-Status und gewähltes Ziel                      |
| ⚙️ **µs-Timersteuerung**      | Stepper laufen so gleichmäßig, dass man sie fast atmen hört                |
| 🧠 **ESP32 Dual-Core**        | Core 1 = Stepper / Core 0 = Display – keine Ruckler, keine Kompromisse       |

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
| **TMC2209 (RA)**       | Dreht um die Rektaszensions-Achse   | STEP 25, DIR 26, EN 27, RX/TX = 16/17 |
| **TMC2209 (DEC)**      | Dreht um die Deklinations-Achse     | STEP 13, DIR 12, EN 14, RX/TX = 4/5   |
| **OLED (SSD1306)**     | Zeigt alles an, außer Mitleid       | I²C: SDA 21, SCL 22                   |
| **RTC (DS3231)**       | Sagt dir, wann du’s verpasst hast   | I²C: SDA 21, SCL 22                   |
| **Joystick (KY-023)**  | Steuert alles intuitiv falsch herum | VRx 34, VRy 35, SW 32                 |
| **SD-Karte**           | Bibliothek mit Lieblingsobjekten    | CS 15, SPI laut Board                 |

---

## 🧠 Softwarestruktur

```
NERDSTAR/
│
├── NERDSTAR.ino           # Orchestriert Setup/Loop
├── catalog.cpp/.h         # SD-Objektbibliothek & Parser
├── display_menu.cpp/.h    # OLED-Menüs, Setup, Goto, Polar Align
├── input.cpp/.h           # Joystick + Encoder Handling
├── motion.cpp/.h          # Stepper-Steuerung, Tracking, Kalibrierung
├── planets.cpp/.h         # Schlanke Planeten-Ephemeriden
├── storage.cpp/.h         # EEPROM & SD Initialisierung
├── config.h               # Pinout & Konstanten
├── data/catalog.xml       # Beispiel-Datenbank für die SD-Karte
├── docs/
│   ├── BEDIENUNGSANLEITUNG.md # Schritt-für-Schritt-Bedienung
│   └── nerdstar-banner.png    # Für die Optik
├── LICENSE
└── README.md
```

---

## 📖 Dokumentation & Daten

- [Bedienungsanleitung](docs/BEDIENUNGSANLEITUNG.md) mit Schritt-für-Schritt-Anweisungen
- Beispiel-Datenbank: [`data/catalog.xml`](data/catalog.xml) – auf die SD-Karte kopieren
- Alle Kalibrierungen & Zustände werden im EEPROM des ESP32 abgelegt

---

## 📺 OLED-Anzeige

```
NERDSTAR Status
RA: 05h 34m 31s
Dec: +22° 00' 52"
Align: Yes  Trk: On
Sel: Messier 042
Goto: --
```

> Wenn du das siehst, weißt du wohin das Teleskop blickt.
> Wenn nicht, hilft die [Bedienungsanleitung](docs/BEDIENUNGSANLEITUNG.md).

---

## 🛰️ Nachführung & Goto

- **Polar Alignment**: eigener Menüpunkt, speichert den Align-Status im EEPROM.
- **Tracking**: siderisches Tracking nach erfolgreicher Ausrichtung per Knopfdruck.
- **Goto**: Auswahl im Katalog, Start im Hauptmenü, Abbruch jederzeit über den Joystick.
- **Planeten**: aktuelle Positionen werden aus der RTC-Zeit berechnet – keine statischen Tabellen.

Kurz gesagt: Der ESP32 weiß, wohin es geht, und bleibt dank Tracking dort.

---

## 🧰 Abhängigkeiten

| Bibliothek         | Zweck                              | Empfohlene Version |
| ------------------ | ---------------------------------- | ------------------ |
| `TMCStepper`       | Kommunikation mit TMC2209          | ≥ 0.7.3            |
| `Adafruit_SSD1306` | OLED-Anzeige                       | ≥ 2.5.9            |
| `Adafruit_GFX`     | Grafik-Backend                     | ≥ 1.11.9           |
| `RTClib`           | DS3231 RTC                         | ≥ 2.1.3            |
| `SD`               | Zugriff auf die Objektbibliothek   | Arduino Core       |

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

GNU General Public License v3.0 © 2025 Philipp

> Benutze, forke, modifiziere – aber denk dran:
> Auch im Weltall gilt: Kein Support bei falscher Polausrichtung.

---

## ✨ Schlusswort

**NERDSTAR** –
Ein Projekt für Menschen, die beim Wort *Stepper*
nicht an Aerobic, sondern an Himmelsmechanik denken.

🤓🌠 *Stay nerdy, track steadily.*
