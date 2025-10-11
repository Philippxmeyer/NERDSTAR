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
| 🧭 **Goto & Catalog**         | Objektbibliothek direkt aus dem EEPROM, Auswahl per Encoder, automatisches Goto |
| 💾 **EEPROM-Katalog**         | 200 vorkonfigurierte Sterne/Nebel/Galaxien/Planeten, beim Start automatisch geladen |
| 🕒 **RTC DS3231**             | Uhrzeit via Setup-Menü setzen, Grundlage für Planetenpositionen             |
| 🪐 **Planetenberechnung**     | Schlanker Algorithmus liefert aktuelle RA/Dec für klassische Planeten       |
| 🔧 **Setup & Kalibrierung**   | Menü für RTC-Zeit, Joystick-Zentrum, Achsenkalibrierung & EEPROM-Speicher   |
| 📺 **OLED Status Display**    | Zeigt RA/Dec, Tracking-/Goto-Status und gewähltes Ziel                      |
| ⚙️ **µs-Timersteuerung**      | Stepper laufen so gleichmäßig, dass man sie fast atmen hört                |
| 🧠 **ESP32 Dual-Core**        | Hauptrechner: Core 1 steuert die Motoren, Core 0 berechnet Kurs & Protokoll |
| 🔌 **Zwei ESP32**             | Zweiter ESP32-WROOM kümmert sich ausschließlich um HID (Display, Joystick, Persistenz) |
| 🔁 **Bulletproof HID-Link**   | Dedizierter UART + Retry-Logik sichert stabile Kommunikation zwischen den Boards |

---

## 💮 Projektziel (philosophisch betrachtet)

> „Wenn’s sich dreht und der Himmel nicht wegläuft, war’s ein Erfolg.“
> — *Philipp, 2025*

NERDSTAR ist modular aufgebaut:
Erst manuell bewegen, dann automatisch nachführen,
und irgendwann sagen: „Lauf, kleiner ESP, lauf mit den Sternen.“

---

## ⚙️ Hardwareübersicht

| Komponente             | Aufgabe                             | Pins / Anschlüsse                     |
| ---------------------- | ----------------------------------- | ------------------------------------- |
| **ESP32 (Hauptrechner)** | Kursberechnung + Motorsteuerung      | UART2 TX (17) ↔ HID-RX, UART2 RX (16) ↔ HID-TX |
| **ESP32-WROOM (HID)**  | Display, Eingaben, EEPROM-Katalog    | UART2 TX (17) ↔ Main-RX, UART2 RX (16) ↔ Main-TX |
| **TMC2209 (RA)**       | Dreht um die Rektaszensions-Achse   | STEP 25, DIR 26, EN 27, MS1/MS2 via Pull-up = 1/16 µSteps |
| **TMC2209 (DEC)**      | Dreht um die Deklinations-Achse     | STEP 13, DIR 12, EN 14, MS1/MS2 via Pull-up = 1/16 µSteps |
| **OLED (SSD1306)**     | Zeigt alles an, außer Mitleid       | I²C: SDA 21, SCL 22 (HID-ESP32-WROOM) |
| **RTC (DS3231)**       | Sagt dir, wann du’s verpasst hast   | I²C: SDA 21, SCL 22 (HID-ESP32-WROOM) |
| **Joystick (KY-023)**  | Steuert alles intuitiv falsch herum | VRx 34, VRy 35, SW 27 (HID-ESP32-WROOM) |
| **Rotary-Encoder**     | Menü & Bestätigungen                | A = 18, B = 19, Button = 23 (HID-ESP32-WROOM) |

### 🔌 Verkabelung im Detail

#### Hauptrechner-ESP32 → Motortreiber

| Signal                   | Pin am ESP32 (Main) | Anschluss am TMC2209 (RA) | Anschluss am TMC2209 (DEC) |
| ------------------------ | ------------------- | ------------------------- | -------------------------- |
| Enable                   | 27                  | EN                        | EN                         |
| Richtung (DIR)           | 26                  | DIR                       | –                          |
| Schritt (STEP)           | 25                  | STEP                      | –                          |
| Richtung (DIR)           | 12                  | –                         | DIR                        |
| Schritt (STEP)           | 13                  | –                         | STEP                       |
| Versorgung & Masse       | 5 V / GND           | VM / GND                  | VM / GND                   |

> Hinweis: PDN/UART (MS1) und MS2 werden nicht mehr zum ESP32 geführt. Ziehe beide Pins per Pull-up (z. B. 10 kΩ nach VIO) auf HIGH, um dauerhaft 1/16 Mikroschritte zu erzwingen.

#### HID-ESP32-WROOM → Benutzerschnittstellen

| Gerät / Signal                  | Pin am ESP32 (HID) | Bemerkung |
| -------------------------------- | ------------------ | --------- |
| OLED + RTC SDA                   | 21                 | Gemeinsamer I²C-Bus |
| OLED + RTC SCL                   | 22                 | Gemeinsamer I²C-Bus |
| Rotary-Encoder A                 | 18                 | INPUT_PULLUP verwenden |
| Rotary-Encoder B                 | 19                 | INPUT_PULLUP verwenden |
| Rotary-Encoder Button            | 23                 | Mit INPUT_PULLUP betreiben |
| Joystick X (VRx)                 | 34                 | ADC, input only |
| Joystick Y (VRy)                 | 35                 | ADC, input only |
| Joystick Button                  | 27                 | LOW-aktiv, interner Pull-up verfügbar |
| Gemeinsame Versorgung für HID    | 3.3 V / GND        | Alle Sensoren/Bedienelemente |

#### Verbindung zwischen den beiden ESP32

- **TX ↔ RX kreuzen:** Main-TX (GPIO 17) → HID-RX (GPIO 16) und Main-RX (GPIO 16) ← HID-TX (GPIO 17)
- **GND verbinden:** Gemeinsamer Bezugspunkt für UART und Sensoren
- Optional: **5 V / 3.3 V** gemeinsam einspeisen, wenn beide Boards aus derselben Quelle versorgt werden
- Hinweis: Der Link nutzt jetzt einen dedizierten Hardware-UART. USB-Debug-Ausgaben laufen parallel weiter, ohne das Protokoll zu stören.

Diese Belegung entspricht exakt den Konstanten in [`config.h`](config.h) und stellt sicher, dass jede Komponente am richtigen Controller hängt.

---

## 🧠 Softwarestruktur

```
NERDSTAR/
│
├── NERDSTAR.ino           # Orchestriert Setup/Loop
├── catalog.cpp/.h         # EEPROM-Katalog & Parser
├── display_menu.cpp/.h    # OLED-Menüs, Setup, Goto, Polar Align
├── input.cpp/.h           # Joystick + Encoder Handling
├── motion_main.cpp/.h     # Stepper-Steuerung & Kursberechnung (Hauptrechner)
├── motion_hid.cpp         # RPC-Proxy für Motion-Funktionen (HID)
├── comm.cpp/.h            # UART-Protokoll zwischen Hauptrechner und HID
├── planets.cpp/.h         # Schlanke Planeten-Ephemeriden
├── storage.cpp/.h         # EEPROM-Konfiguration & Katalogspeicher
├── config.h               # Pinout & Konstanten
├── data/catalog.xml       # Quellliste für den eingebauten Katalog
├── docs/
│   ├── BEDIENUNGSANLEITUNG.md # Schritt-für-Schritt-Bedienung
│   └── nerdstar-banner.png    # Für die Optik
├── LICENSE
└── README.md
```

---

### Firmware-Varianten

- **HID-Firmware (Standard)**: Ohne zusätzliche Defines kompilieren. Baut das
  UI für Display, Joystick, Katalog und spricht den Hauptrechner per UART an.
- **Hauptrechner-Firmware**: In den Compiler-Optionen `DEVICE_ROLE_MAIN`
  definieren (z.B. `-DDEVICE_ROLE_MAIN`). Der Code initialisiert die
  Schrittmotoren, startet zwei Tasks (Core 0 = Kursberechnung & Protokoll,
  Core 1 = Motorsteuerung) und beantwortet alle Motion-RPCs.

Beide Varianten sprechen über einen dedizierten Hardware-UART: Der
Hauptrechner nutzt **UART2 auf GPIO17/GPIO16**, die HID-Variante mit dem
ESP32-WROOM DevKit ebenso **UART2 auf GPIO17/GPIO16**. USB-Debug läuft
parallel über den integrierten Serial-Port und bleibt störungsfrei.

---

## 📖 Dokumentation & Daten

- [Bedienungsanleitung](docs/BEDIENUNGSANLEITUNG.md) mit Schritt-für-Schritt-Anweisungen
- Beispiel-Datenbank: [`data/catalog.xml`](data/catalog.xml) – dient als Quelle für den eingebauten EEPROM-Katalog
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
| `Adafruit_SSD1306` | OLED-Anzeige                       | ≥ 2.5.9            |
| `Adafruit_GFX`     | Grafik-Backend                     | ≥ 1.11.9           |
| `RTClib`           | DS3231 RTC                         | ≥ 2.1.3            |

---

## 🛠 Debugging

- Beide Controller initialisieren den USB-Seriell-Port mit **115200 Baud**.
- Der HID-Controller loggt den Verbindungsstatus (`Mount link ready/offline`).
- Der Hauptrechner bestätigt den Start beider Tasks und meldet etwaige RPC-Retrys mit `[COMM]` auf der Konsole.

---

## ⚡ Installation

1. Arduino IDE öffnen
2. Boards wählen:
   - HID-Controller: **ESP32 Dev Module** (ESP32-WROOM DevKit)
   - Hauptrechner: **ESP32 Dev Module**
3. Bibliotheken installieren (`Adafruit_SSD1306`, `Adafruit_GFX`, `RTClib`)
4. Mikroschritt-Pins setzen: PDN/UART (MS1) & MS2 der TMC2209 per Pull-up auf VIO (1/16 µSteps)
5. **HID-ESP32-WROOM** flashen (ohne zusätzliche Build-Flags)
6. **Hauptrechner-ESP32** flashen (Build-Flag `-DDEVICE_ROLE_MAIN` setzen)
7. UART kreuzen: Main-TX17 ↔ HID-RX16, Main-RX16 ↔ HID-TX17, GND verbinden
8. Kaffee holen
9. Freuen, dass du was gebaut hast, das klingt wie ein NASA-Projekt und aussieht wie ein Nerd-Traum.

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
