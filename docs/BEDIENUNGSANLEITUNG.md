# Bedienungsanleitung NERDSTAR

Diese Anleitung führt dich Schritt für Schritt durch Inbetriebnahme und Bedienung des NERDSTAR-Teleskopcontrollers.

## 1. Vorbereitung

1. **Hardware prüfen**
   - ESP32 mit TMC2209-Treibern für Azimut- und Höhenachse (Alt/Az)
   - OLED-Display (SSD1306), Rotary-Encoder, KY-023-Joystick
   - DS3231-RTC (I²C)
2. **Katalog**
   - Der komplette Objektkatalog liegt fest im EEPROM. Eine SD-Karte ist nicht mehr erforderlich.
   - Anpassungen erfolgen über [`data/catalog.xml`](../data/catalog.xml); anschließend muss die Firmware neu gebaut werden.
   - Beim Bauen entsteht daraus `catalog_data.inc`, das in die HID-Firmware eingebettet wird. Beim ersten Start schreibt die Firmware den Katalog automatisch in den emulierten EEPROM-Bereich des ESP32 (4 KB Flash). Dort bleibt er auch nach Firmware-Updates erhalten.
3. **WLAN (optional)**
   - Zugangsdaten können in [`data/eeprom_template.json`](../data/eeprom_template.json) hinterlegt werden.
   - Beim Flashen der vorbereiteten EEPROM-Daten stehen die Credentials beiden Controllern für WiFi OTA & NTP zur Verfügung.【F:data/eeprom_template.json†L1-L6】
4. **Geräterolle wählen**
   - In [`role_config.h`](../role_config.h) wird gesteuert, welche Variante kompiliert wird. Standardmäßig ist `DEVICE_ROLE_HID` aktiv.
   - Für das Hauptsteuerungs-Board beim Kompilieren den Define `DEVICE_ROLE_MAIN` setzen (z. B. `arduino-cli compile --build-property build.extra_flags=-DDEVICE_ROLE_MAIN`). Alternativ kann der Define temporär vor dem `#include "role_config.h"` in `NERDSTAR.ino` ergänzt werden.
   - Nach dem Flashen der Main-Firmware wieder den HID-Build ohne zusätzliche Defines kompilieren, um die HID-Einheit zu aktualisieren.
5. **Verdrahtung gemäß Pinbelegung**
   - **ESP32 (Hauptrechner)** → Steppertreiber
     - RA-Treiber: STEP 25, DIR 26, EN 27
     - DEC-Treiber: STEP 13, DIR 12, EN 14
     - PDN/UART (MS1) & MS2 der TMC2209 mit Pull-ups (z. B. 10 kΩ nach VIO) auf HIGH legen → 1/16 Mikroschritt fest eingestellt
     - Gemeinsame Versorgung 5 V und GND zu beiden TMC2209-Modulen
   - **ESP32-WROOM (HID)** → Eingabe- & Anzeigeeinheit
     - OLED & RTC: SDA 21 / SCL 22 (I²C, 3.3 V/GND)
     - Rotary-Encoder: A 18, B 19, Button 23
     - Joystick: VRx 34, VRy 35, Button 27 (LOW-aktiv, interner Pull-up verfügbar)
   - **ESP32 ↔ ESP32-WROOM (UART-Link)**
     - Main-TX (17) → HID-RX (16), Main-RX (16) ← HID-TX (17)
     - Gemeinsame Masse verbinden (GND ↔ GND)
     - Hinweis: Der Link nutzt einen dedizierten Hardware-UART. USB-Debug-Ausgaben bleiben unabhängig.
6. **Firmware flashen**
   - Bibliotheken installieren (`Adafruit_SSD1306`, `Adafruit_GFX`, `RTClib`)
   - Sketch `NERDSTAR.ino` mit den neuen Modulen kompilieren und auf die Boards flashen (HID & Main: Board `ESP32 Dev Module`).
   - USB-Seriell (115200 Baud) zeigt beim Booten Statusmeldungen beider Controller.

## 2. Erstinbetriebnahme

1. **Systemstart**
   - Nach dem Booten meldet sich das OLED mit "NERDSTAR ready".
2. **Joystick-Kalibrierung**
   - Beim ersten Start läuft automatisch eine Mittelwert-Kalibrierung.
   - Der Mittelpunkt wird im EEPROM gespeichert und kann jederzeit über das Setup-Menü neu gesetzt werden.
3. **Achsen kalibrieren (optional, empfohlen)**
   - Über `Setup → Cal Axes` lässt sich die Schrittauflösung exakt einstellen.
   - Schritte siehe Abschnitt 5.4.
4. **RTC-Uhrzeit prüfen**
   - Falls die RTC noch nicht gesetzt ist, `Setup → Set RTC` wählen.

## 3. Menüsteuerung

- **Rotary-Encoder drehen** → Menüpunkt wechseln / Werte anpassen
- **Rotary-Encoder drücken** → Auswahl bestätigen / Goto starten / RTC speichern
- **Joystick drücken** → Kontextabhängig (z. B. zurück zum Hauptmenü, Abbruch, Stop)
- **Joystick bewegen** → Manuelles Nachführen, sofern kein Goto aktiv ist

## 4. Hauptmenü

| Menüpunkt        | Funktion                                                                                   |
| ---------------- | ------------------------------------------------------------------------------------------- |
| Status           | Zeigt Az/Alt, Align-/Tracking-Status, gewähltes Ziel sowie Diagnosewerte an.【F:display_menu.cpp†L565-L610】 |
| Polar Align      | Führt durch die Polaris-Ausrichtung. Encoder drücken = bestätigen, Joystick drücken = Abbruch |
| Start Tracking   | Aktiviert siderische Nachführung (nur nach erfolgreicher Ausrichtung)                       |
| Stop Tracking    | Stoppt die Nachführung                                                                      |
| Catalog          | Erst Kategorie wählen (z.B. Planet/Stern), dann Objekt blättern; Encoder = Goto, Joystick = zurück zur Kategorie |
| Goto Selected    | Startet die automatische Bewegung zum zuletzt gemerkten Objekt (siehe Abschnitt 6)          |
| Goto RA/Dec      | Manuelle Zielkoordinaten eingeben und direkt anfahren (siehe Abschnitt 6).【F:display_menu.cpp†L1287-L1446】 |
| Park             | Fährt die Höhe sicher auf das obere Limit und stoppt die Bewegung.【F:display_menu.cpp†L2045-L2086】 |
| Setup            | Öffnet das Setup-Menü                                                                       |

## 5. Setup-Menü

### 5.1 RTC einstellen
1. `Setup → Set RTC` wählen.
2. Encoder drehen verändert den jeweils markierten Wert (Jahr → Monat → … → Sekunde).
3. Joystick drücken springt zum nächsten Feld (inklusive DST-Auswahl „Off/On/Auto“).【F:display_menu.cpp†L748-L803】
4. Encoder drücken speichert Zeit & DST-Modus im RTC/EEPROM.【F:display_menu.cpp†L1587-L1611】

### 5.2 Standort setzen
1. `Setup → Set Location` aufrufen.
2. Encoder drehen passt Breitengrad, Längengrad (in 0,1°) sowie die Zeitzone (15-Minuten-Schritte) an.【F:display_menu.cpp†L805-L854】
3. Joystick drücken wechselt zum nächsten Feld, inklusive „Save/Back“.
4. Encoder drücken bestätigt; „Save“ schreibt die Werte dauerhaft in den EEPROM.【F:display_menu.cpp†L1238-L1284】

### 5.3 Joystick kalibrieren
1. `Setup → Cal Joystick` wählt den Kalibriermodus.
2. Den Joystick loslassen, damit er mittig steht.
3. Nach ca. einer Sekunde werden neue Mittelwerte angezeigt und gespeichert.

### 5.4 Achsen kalibrieren
1. `Setup → Cal Axes` starten. Die Anzeige führt durch vier Schritte:
   - **Set Az 0deg**: Teleskop in Richtung geografischer Norden (Azimut 0°) stellen, Encoder drücken.
   - **Rotate +90deg**: Mit Joystick/Encoder exakt 90° nach Osten drehen, Encoder drücken.
   - **Set Alt 0deg**: Teleskop auf Horizont (Alt 0°) ausrichten, Encoder drücken.
   - **Rotate +45deg**: Genau +45° in der Höhe anheben, Encoder drücken.
2. Die Software berechnet Schritte pro Grad für Azimut und Höhe, setzt Nullpunkte und speichert alles im EEPROM.
3. Bei inkonsistenten Werten erscheint "Cal failed" – Vorgang ggf. wiederholen.

### 5.5 Goto-Geschwindigkeit einstellen
1. `Setup → Goto Speed` öffnet die Parameter für Maximalgeschwindigkeit, Beschleunigung und Abbremsen (jeweils in °/s bzw. °/s²).【F:display_menu.cpp†L1051-L1079】
2. Mit dem Encoder den markierten Wert ändern, Joystick drücken wechselt zum nächsten Feld.【F:display_menu.cpp†L1182-L1218】
3. Encoder drücken speichert das Profil dauerhaft. Angepasste Werte wirken auf beide Achsen.【F:display_menu.cpp†L1219-L1234】

### 5.6 Pan-Geschwindigkeit einstellen
1. `Setup → Pan Speed` bearbeiten, um Joystick-Fahrten anzupassen (Aufbau wie `Goto Speed`).【F:display_menu.cpp†L1171-L1179】【F:display_menu.cpp†L1051-L1079】
2. Werte gelten für manuelles Slewen beider Achsen.【F:display_menu.cpp†L1182-L1234】

### 5.7 Umkehrspiel (Backlash) kalibrieren
1. `Setup → Cal Backlash` starten. Die Anzeige führt durch die vier Messpunkte:
   - **Az fwd pos**: Objekt mit Joystick in Vorwärtsrichtung anfahren, Encoder drücken.
   - **Az reverse**: Richtung wechseln bis Spiel aufgehoben ist, Encoder drücken.
   - **Alt fwd pos** / **Alt reverse**: Gleiches Verfahren für die Höhenachse.
2. Zwischendurch den Joystick für die Bewegung nutzen, Joystick-Taste bricht den Vorgang ab.
3. Nach Abschluss werden die ermittelten Schrittwerte gespeichert und automatisch in der Goto-Steuerung berücksichtigt.

### 5.8 WiFi OTA
1. `Setup → WiFi OTA` zeigt den aktuellen Status (`NoCfg`, `Off`, `Conn`, `On`).【F:display_menu.cpp†L724-L737】
2. Encoder drücken schaltet WLAN für beide ESP32 um, sofern Credentials vorhanden sind. Bei Erfolg erscheinen SSID oder Fehlerhinweise.【F:display_menu.cpp†L2248-L2281】
3. Aktives WLAN ermöglicht OTA-Updates und wiederholte NTP-Synchronisation der Systemzeit.【F:wifi_ota.cpp†L64-L149】

## 6. Katalog und Goto

1. `Catalog` öffnen, mit dem Encoder zuerst die gewünschte Kategorie (z.B. Planet, Stern, Nebel) wählen.
2. Encoder drücken öffnet die Objektliste der Kategorie; dort mit dem Encoder blättern.
3. Encoder drücken startet unmittelbar ein Goto zum angezeigten Objekt. Die Software plant die Ankunftszeit vor, damit Zielkoordinaten zum Ende der Fahrt stimmen.
4. Ziele unter dem Horizont werden automatisch blockiert (Anzeige "Below horizon").
5. Während eines Goto ist Tracking deaktiviert; Joystick-Taste bricht jederzeit ab.
6. Nach Abschluss erscheinen "Goto done" und die Nachführung wird automatisch mit dem Ziel als Referenz aktiviert.
7. Im Hauptmenü kann `Goto Selected` genutzt werden, um das zuletzt gewählte Objekt erneut anzufahren.

**Manuelles Goto**: Über `Goto RA/Dec` lassen sich Koordinaten (RA hh:mm:ss, Dec ±dd:mm:ss) direkt eingeben. Der Joystick wechselt das Feld, der Encoder verändert die Werte. Nach Bestätigung fährt der Controller die eingegebene Position an.【F:display_menu.cpp†L1287-L1446】

**Planeten**: Für Objekte mit Typ `Planet` wird die RA/Dec über die eingebaute Planetenberechnung (basierend auf Julianischem Datum) bestimmt. Voraussetzung: RTC läuft.

## 7. Polaris-Ausrichtung und Tracking

1. `Polar Align` wählen, mit Joystick auf Polaris zentrieren.
2. Encoder drücken → Referenzwerte werden gesetzt und gespeichert.
3. Nach erfolgreichem Align `Start Tracking` aktivieren.
4. Während laufender Nachführung darf der Joystick jederzeit zur Feinjustage bewegt werden. Sobald der Joystick losgelassen wird, übernimmt das System den neuen Versatz und führt den zuletzt angefahrenen Punkt automatisch nach.

**Parken**: Der Menüpunkt `Park` fährt die Höhenachse zur sicheren Maximalposition und stoppt anschließend alle Bewegungen – ideal für Transport oder Abschaltung.【F:display_menu.cpp†L2045-L2086】

## 8. Sicherheit & Tipps

- **Not-Stopp**: Joystick drücken stoppt alle Motoren, Tracking wird deaktiviert.
- **Horizontschutz**: Goto-Kommandos unterhalb des mathematischen Horizonts werden automatisch verhindert.
- **EEPROM**: Konfiguration (Kalibrierungen, RTC-Zeitstempel, Align-Status) wird automatisch gesichert.
- **Planeten-Update**: Für exakte Positionen sollte die RTC regelmäßig synchronisiert werden.

Viel Spaß beim Erkunden des Himmels mit NERDSTAR! 🤓🌌
