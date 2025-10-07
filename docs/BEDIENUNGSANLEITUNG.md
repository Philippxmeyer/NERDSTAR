# Bedienungsanleitung NERDSTAR

Diese Anleitung führt dich Schritt für Schritt durch Inbetriebnahme und Bedienung des NERDSTAR-Teleskopcontrollers.

## 1. Vorbereitung

1. **Hardware prüfen**
   - ESP32 mit TMC2209-Treibern für Azimut- und Höhenachse (Alt/Az)
   - OLED-Display (SSD1306), Rotary-Encoder, KY-023-Joystick
   - DS3231-RTC und Micro-SD-Karte (FAT32 formatiert)
2. **SD-Karte bestücken**
   - Kopiere die Datei [`data/catalog.xml`](../data/catalog.xml) auf die SD-Karte.
   - Die Datei enthält 200 Beispielobjekte (Sterne, Messier-Objekte, Planeten) im XML-Format und kann bei Bedarf erweitert werden.
3. **Verdrahtung gemäß Pinbelegung**
   - Pins siehe Tabelle im README (I²C an GPIO 21/22, SD-CS an GPIO 15 usw.).
4. **Firmware flashen**
   - Bibliotheken installieren (`TMCStepper`, `Adafruit_SSD1306`, `Adafruit_GFX`, `RTClib`, `SD`)
   - Sketch `NERDSTAR.ino` mit den neuen Modulen kompilieren und auf den ESP32 flashen.

## 2. Erstinbetriebnahme

1. **Systemstart**
   - Nach dem Booten meldet sich das OLED mit "NERDSTAR ready".
2. **Joystick-Kalibrierung**
   - Beim ersten Start läuft automatisch eine Mittelwert-Kalibrierung.
   - Der Mittelpunkt wird im EEPROM gespeichert und kann jederzeit über das Setup-Menü neu gesetzt werden.
3. **Achsen kalibrieren (optional, empfohlen)**
   - Über `Setup → Cal Axes` lässt sich die Schrittauflösung exakt einstellen.
   - Schritte siehe Abschnitt 5.
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
| Status           | Zeigt RA/Dec, Align-/Tracking-Status und ausgewähltes Ziel                                  |
| Polar Align      | Führt durch die Polaris-Ausrichtung. Encoder drücken = bestätigen, Joystick drücken = Abbruch |
| Start Tracking   | Aktiviert siderische Nachführung (nur nach erfolgreicher Ausrichtung)                       |
| Stop Tracking    | Stoppt die Nachführung                                                                      |
| Catalog          | Blättert durch alle Objekte; Encoder = sofortiges Goto, Joystick = zurück                      |
| Goto Selected    | Startet die automatische Bewegung zum zuletzt gemerkten Objekt (siehe Abschnitt 6)          |
| Setup            | Öffnet das Setup-Menü                                                                       |

## 5. Setup-Menü

### 5.1 RTC einstellen
1. `Setup → Set RTC` wählen.
2. Encoder drehen verändert den jeweils markierten Wert (Jahr → Monat → … → Sekunde).
3. Joystick drücken springt zum nächsten Feld.
4. Encoder drücken speichert die Zeit (RTC und EEPROM).

### 5.2 Joystick kalibrieren
1. `Setup → Cal Joystick` wählt den Kalibriermodus.
2. Den Joystick loslassen, damit er mittig steht.
3. Nach ca. einer Sekunde werden neue Mittelwerte angezeigt und gespeichert.

### 5.3 Achsen kalibrieren
1. `Setup → Cal Axes` starten. Die Anzeige führt durch vier Schritte:
   - **Set Az 0deg**: Teleskop in Richtung geografischer Norden (Azimut 0°) stellen, Encoder drücken.
   - **Rotate +90deg**: Mit Joystick/Encoder exakt 90° nach Osten drehen, Encoder drücken.
   - **Set Alt 0deg**: Teleskop auf Horizont (Alt 0°) ausrichten, Encoder drücken.
   - **Rotate +45deg**: Genau +45° in der Höhe anheben, Encoder drücken.
2. Die Software berechnet Schritte pro Grad für Azimut und Höhe, setzt Nullpunkte und speichert alles im EEPROM.
3. Bei inkonsistenten Werten erscheint "Cal failed" – Vorgang ggf. wiederholen.

### 5.4 Goto-Geschwindigkeit einstellen
1. `Setup → Goto Speed` öffnet die Parameter für Maximalgeschwindigkeit, Beschleunigung und Abbremsen (jeweils in °/s bzw. °/s²).
2. Mit dem Encoder den markierten Wert ändern, Joystick drücken wechselt zum nächsten Feld.
3. Encoder drücken speichert das Profil dauerhaft. Angepasste Werte wirken auf beide Achsen.

### 5.5 Umkehrspiel (Backlash) kalibrieren
1. `Setup → Cal Backlash` starten. Die Anzeige führt durch die vier Messpunkte:
   - **Az fwd pos**: Objekt mit Joystick in Vorwärtsrichtung anfahren, Encoder drücken.
   - **Az reverse**: Richtung wechseln bis Spiel aufgehoben ist, Encoder drücken.
   - **Alt fwd pos** / **Alt reverse**: Gleiches Verfahren für die Höhenachse.
2. Zwischendurch den Joystick für die Bewegung nutzen, Joystick-Taste bricht den Vorgang ab.
3. Nach Abschluss werden die ermittelten Schrittwerte gespeichert und automatisch in der Goto-Steuerung berücksichtigt.

## 6. Katalog und Goto

1. `Catalog` öffnen, mit dem Encoder durch die Liste blättern.
2. Encoder drücken startet unmittelbar ein Goto zum angezeigten Objekt. Die Software plant die Ankunftszeit vor, damit Zielkoordinaten zum Ende der Fahrt stimmen.
3. Ziele unter dem Horizont werden automatisch blockiert (Anzeige "Below horizon").
4. Während eines Goto ist Tracking deaktiviert; Joystick-Taste bricht jederzeit ab.
5. Nach Abschluss erscheinen "Goto done" und die Nachführung wird automatisch mit dem Ziel als Referenz aktiviert.
6. Im Hauptmenü kann `Goto Selected` genutzt werden, um das zuletzt gewählte Objekt erneut anzufahren.

**Planeten**: Für Objekte mit Typ `Planet` wird die RA/Dec über die eingebaute Planetenberechnung (basierend auf Julianischem Datum) bestimmt. Voraussetzung: RTC läuft.

## 7. Polaris-Ausrichtung und Tracking

1. `Polar Align` wählen, mit Joystick auf Polaris zentrieren.
2. Encoder drücken → Referenzwerte werden gesetzt und gespeichert.
3. Nach erfolgreichem Align `Start Tracking` aktivieren.
4. Während laufender Nachführung darf der Joystick jederzeit zur Feinjustage bewegt werden. Sobald der Joystick losgelassen wird, übernimmt das System den neuen Versatz und führt den zuletzt angefahrenen Punkt automatisch nach.

## 8. Sicherheit & Tipps

- **Not-Stopp**: Joystick drücken stoppt alle Motoren, Tracking wird deaktiviert.
- **Horizontschutz**: Goto-Kommandos unterhalb des mathematischen Horizonts werden automatisch verhindert.
- **SD-Karte**: Änderungen an `catalog.xml` nur bei ausgeschaltetem Gerät oder mit sicherem Entfernen.
- **EEPROM**: Konfiguration (Kalibrierungen, RTC-Zeitstempel, Align-Status) wird automatisch gesichert.
- **Planeten-Update**: Für exakte Positionen sollte die RTC regelmäßig synchronisiert werden.

Viel Spaß beim Erkunden des Himmels mit NERDSTAR! 🤓🌌
