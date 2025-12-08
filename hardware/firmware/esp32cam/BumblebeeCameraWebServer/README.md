# ESP32-CAM BeeSense Firmware

## Übersicht
Arduino-basierte Firmware für ESP32-CAM zur Aufnahme und Speicherung von Hummel-Beobachtungsbildern.

## Funktionsumfang

### Hardware-Setup
- **Kamera**: ESP32-CAM mit VGA-Auflösung (640×480)
- **Speicher**: SD-Karte über SPI-Interface
- **Netzwerk**: WLAN-Verbindung (2.4 GHz)

### Kernfunktionen

#### 1. Kamera-Initialisierung
- VGA-Auflösung (640×480 Pixel)
- JPEG-Kompression
- Optimiert für ESP32-CAM AI-Thinker Modul

#### 2. SD-Karten-Anbindung
- SPI-Kommunikation mit SD-Karte
- Automatische Ordnerstruktur-Erstellung
- Bilder werden in `/BeeSense` gespeichert
- Dateiname-Format: `YYYY-MM-DD_HH-MM-SS-xxx.jpg`

#### 3. WLAN-Verbindung
- Verbindung mit lokalem Netzwerk (FRITZ!Box)
- IP-Adresse wird im Serial Monitor ausgegeben
- Webserver erreichbar über Browser

#### 4. NTP-Zeitabgleich
- Automatischer Zeitabgleich über NTP-Server
- Zeitzone: Europe/Berlin
- Exakte Zeitstempel für alle Aufnahmen
- Keine EEPROM-Nutzung mehr für Zeitstempel erforderlich

#### 5. Webserver mit HTML-Oberfläche
- **Live-Vorschau**: Aktualisiert alle 300ms (~3 FPS)
- **Einzelbild-Aufnahme**: Button speichert aktuelles Foto auf SD-Karte
- **Statusmeldungen**: Direktes Feedback im Browser
- **Debug-Ausgaben**: Detaillierte Logs im Serial Monitor

### Geplante Erweiterungen
- **Serienaufnahme**: Button "Serie 5s @ 3 FPS" (ca. 15 Bilder in Folge)
- Automatische Upload-Funktionalität für KI-Inferenz

## Dateistruktur

```
esp32cam_beesense/
├── esp32cam_beesense.ino      # Haupt-Arduino-Sketch
├── config.h                   # Konfigurationsdatei (WLAN, etc.)
└── README.md                  # Diese Datei
```

## Installation

### 1. Arduino IDE Setup
1. Arduino IDE 2.x installieren
2. ESP32 Board Support hinzufügen:
   - **File → Preferences**
   - **Additional Board Manager URLs**: 
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
3. **Tools → Board → Board Manager** → "ESP32" suchen und installieren
4. **Tools → Board** → "AI Thinker ESP32-CAM" auswählen

### 2. Erforderliche Bibliotheken
Alle Bibliotheken sind im ESP32 Core enthalten:
- `WiFi.h` - WLAN-Funktionalität
- `esp_camera.h` - Kamera-Steuerung
- `SD_MMC.h` oder `SD.h` - SD-Karten-Zugriff
- `time.h` - NTP-Zeitabgleich
- `WebServer.h` - HTTP-Server
- `EEPROM.h` - Persistenter Speicher (optional)

### 3. Hardware-Verkabelung

#### ESP32-CAM Pinout (AI-Thinker)
Kamera-Pins sind fest verdrahtet auf dem Modul.

#### SD-Karte
- SD-Karten-Slot ist auf dem ESP32-CAM Modul integriert
- Unterstützt bis zu 32GB (FAT32)
- Empfohlen: Class 10 MicroSD-Karte

#### Programmierung
**WICHTIG**: ESP32-CAM hat keinen integrierten USB-Programmer!

**Upload via FTDI Programmer:**
```
FTDI        →  ESP32-CAM
GND         →  GND
5V (oder 3.3V) → 5V
TX          →  U0R (RX)
RX          →  U0T (TX)
```

**Upload-Modus aktivieren:**
1. GPIO0 mit GND verbinden (z.B. mit Jumper)
2. Reset-Button drücken
3. Upload in Arduino IDE starten
4. Nach erfolgreichem Upload: GPIO0 von GND trennen
5. Reset-Button drücken → Normaler Betrieb

### 4. Konfiguration

Öffne die `.ino` Datei und passe folgende Einstellungen an:

```cpp
// WLAN-Zugangsdaten
const char* ssid = "DEIN_WLAN_NAME";
const char* password = "DEIN_WLAN_PASSWORT";

// NTP-Server (optional anpassen)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;        // GMT+1 (Deutschland)
const int daylightOffset_sec = 3600;    // Sommerzeit

// SD-Karten Ordner
const char* photoFolder = "/BeeSense";
```

## Verwendung

### 1. Serial Monitor
- **Baud Rate**: 115200
- Zeigt:
  - Initialisierungsstatus
  - WLAN-Verbindungsstatus
  - Zugewiesene IP-Adresse
  - NTP-Synchronisation
  - Aufnahme-Events

### 2. Weboberfläche
1. ESP32-CAM starten
2. IP-Adresse aus Serial Monitor ablesen (z.B. `192.168.178.45`)
3. Browser öffnen und IP eingeben: `http://192.168.178.45`
4. Webinterface wird angezeigt mit:
   - **Live-Vorschau** (automatisch aktualisiert)
   - **Button "Einzelbild speichern"** → Foto wird auf SD gespeichert
   - **Statusmeldung** mit Dateinamen

### 3. Bilder von SD-Karte abrufen
- SD-Karte aus ESP32 entfernen
- Mit PC verbinden (SD-Kartenleser)
- Ordner `/BeeSense` öffnen
- Bilder im Format: `2025-12-08_14-32-15-001.jpg`

## Energieversorgung

- **Stromverbrauch**:
  - Standby: ~50-80 mA
  - Foto-Aufnahme: ~250-350 mA
  - WLAN aktiv: ~80-150 mA

- **Empfehlung**: 5V/2A Netzteil (Micro-USB oder direkt an 5V Pin)

## Troubleshooting

### Kamera initialisiert nicht
- Reset-Button drücken
- SD-Karte entfernen und neu starten (um Kamera-Pin-Konflikte auszuschließen)
- Andere Kamera-Konfiguration probieren (z.B. geringere Auflösung)

### WLAN verbindet nicht
- SSID und Passwort überprüfen
- Nur 2.4 GHz WLAN wird unterstützt (nicht 5 GHz!)
- Router-Firewall prüfen
- Serial Monitor für Fehlerausgaben checken

### SD-Karte wird nicht erkannt
- Karte formatieren (FAT32)
- Maximal 32GB verwenden
- Class 10 oder höher empfohlen
- Karte richtig eingesteckt? (Kontakte nach unten)

### Zeitstempel falsch
- WLAN-Verbindung erforderlich für NTP
- NTP-Server erreichbar?
- Zeitzone korrekt eingestellt?

### Webserver nicht erreichbar
- IP-Adresse korrekt?
- ESP32 und PC im gleichen Netzwerk?
- Firewall prüfen
- Serial Monitor für IP-Adresse überprüfen

## Technische Details

### Dateinamen-Format
```
YYYY-MM-DD_HH-MM-SS-XXX.jpg
2025-12-08_14-32-15-001.jpg
```
- Jahr-Monat-Tag_Stunde-Minute-Sekunde-Millisekunden

### Bildqualität
- Auflösung: 640×480 (VGA)
- Format: JPEG
- Qualität: Konfigurierbar (Standard: 10-12)
- Dateigröße: ~30-80 KB pro Bild

### Netzwerk-Protokolle
- HTTP für Webserver
- NTP für Zeitsynchronisation
- Kein HTTPS (ESP32 Resource-Limitierung)

## Zukünftige Entwicklung

### Geplante Features
- ✅ Serienaufnahme (5 Sekunden @ 3 FPS)
- 🔄 Automatischer Upload zu Backend-API
- 🔄 Bewegungserkennung (PIR-Sensor Integration)
- 🔄 Deep Sleep Modus für Batteriebetrieb
- 🔄 MQTT-Integration für IoT-Plattformen
- 🔄 Lokale Vorfilterung (Helligkeits-/Bewegungsdetektion)

## Lizenz
Siehe Haupt-Repository LICENSE

## Kontakt
BeeSense Projekt - Automatisierte Hummel-Erkennung mit KI
