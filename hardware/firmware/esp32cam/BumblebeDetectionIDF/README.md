# BeeSense - ESP32-S3 Bumblebee Detection System

Ein Echtzeit-Bienenerkennung-System basierend auf ESP32-S3-EYE Hardware mit YOLOv8n Deep Learning Model.

## Übersicht

Dieses Projekt implementiert ein Edge-AI-System zur Erkennung von Hummeln mittels einer ESP32-S3-Kamera. Das System nutzt ein quantisiertes YOLOv8n-Modell, das speziell für Embedded-Geräte mit ESP-DL Framework optimiert wurde.

### Features

**Implementiert:**
- Live MJPEG-Stream mit QVGA Auflösung (320x240)
- Fotospeicherung auf SD-Karte mit automatischer Nummerierung
- WiFi-Verbindung und Web-Interface
- NTP-Zeitsynchronisation (Timezone: Europe/Berlin)
- Modernes Dark-Theme Web-UI
- Heap-Monitoring und Systemstabilität

**In Entwicklung:**
- YOLOv8n Inference-Integration (Modell eingebettet, Code vorbereitet)
- Echtzeit-Hummelerkennung mit Bounding Boxes
- Erkennungszähler und Statistiken

## Hardware

### Benötigte Komponenten
- **ESP32-S3-EYE** (oder kompatibles Board)
  - ESP32-S3 Chip mit PSRAM
  - OV2640 Kamera-Modul
  - 8MB Flash
- **MicroSD-Karte** (SDHC, getestet mit 15GB)
- **USB-Kabel** (zum Flashen und Monitoring)

### Pin-Konfiguration

**Kamera (OV2640):**
- PWDN: GPIO 46
- RESET: nicht verwendet
- XCLK: GPIO 15 (10MHz)
- SIOD: GPIO 4
- SIOC: GPIO 5
- Y9-Y2: GPIO 16,17,18,12,10,8,9,11
- VSYNC: GPIO 6
- HREF: GPIO 7
- PCLK: GPIO 13

**SD-Karte (SPI):**
- MISO: GPIO 40
- MOSI: GPIO 38
- SCLK: GPIO 39
- CS: GPIO 41
- ENABLE: GPIO 48

## Software-Voraussetzungen

### ESP-IDF Installation
```powershell
# ESP-IDF v5.5.1 mit Python 3.11
# Download von: https://dl.espressif.com/dl/esp-idf/

# Nach Installation:
C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1
```

### Dependencies
Die folgenden Komponenten werden automatisch über den IDF Component Manager installiert:
- `espressif/esp32-camera ^2.1.4` - Kamera-Treiber
- `espressif/esp-dl ^1.1.0` - Deep Learning Framework
- `espressif/esp_jpeg ^1.3.1` - JPEG-Encoder

## Build und Flash

### 1. ESP-IDF Environment aktivieren
```powershell
cd C:\Users\nilsg\repos\BeeSense\hardware\firmware\esp32cam\BumblebeDetectionIDF
C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1
```

### 2. Projekt konfigurieren (optional)
```powershell
idf.py menuconfig
```

Wichtige Einstellungen (bereits in `sdkconfig.defaults`):
- SPIRAM aktiviert (Octal Mode)
- Flash Size: 8MB
- Main Task Stack: 8192 bytes

### 3. WiFi-Credentials anpassen
Editiere `main/app_main.cpp`:
```cpp
#define WIFI_SSID "DEIN_WIFI_NAME"
#define WIFI_PASS "DEIN_WIFI_PASSWORT"
```

### 4. Build
```powershell
idf.py build
```

**Build-Output:** `build/bumblebee-detection.bin` (~1MB)

### 5. Flash
```powershell
# Automatische Port-Erkennung
idf.py flash

# Oder mit spezifischem Port
idf.py -p COM3 flash
```

### 6. Serial Monitor
```powershell
idf.py -p COM3 monitor

# Beenden: Strg+]
```

## Verwendung

### 1. System starten
Nach dem Flash bootet das System automatisch:
```
I (xxx) APP: WiFi initialized
I (xxx) APP: Connected to FRITZ!Box 7560 LC GB
I (xxx) APP: IP: 192.168.178.34
I (xxx) NTP: Time synchronized
I (xxx) SDCARD: SD card mounted successfully
I (xxx) APP: Camera initialized (QVGA)
I (xxx) APP: HTTP Server started on port 80
I (xxx) APP: Stream Server started on port 81
```

### 2. Web-Interface öffnen
```
http://192.168.178.34
```

### 3. Funktionen nutzen

**Live-Stream:**
- Der MJPEG-Stream wird automatisch beim Öffnen der Seite geladen
- Auflösung: 320x240 (QVGA)
- Framerate: ~4-8 FPS (abhängig von Netzwerk)

**Foto speichern:**
1. Button "📸 Foto speichern" klicken
2. Aktueller Frame wird als JPEG auf SD-Karte gespeichert
3. Dateiname: `photo000.jpg`, `photo001.jpg`, etc.
4. Speicherort: `/sdcard/` (SD-Root)

**Hummeln erkennen (Mock):**
1. Button "🐝 Hummeln erkennen" klicken
2. Aktuell: Mock-Funktion gibt 0 zurück
3. Foto wird gespeichert als: `detect000_n0.jpg`
4. TODO: Echte YOLOv8-Inference integrieren

### 4. Fotos von SD-Karte abrufen
1. ESP32 ausschalten
2. SD-Karte entnehmen
3. Fotos im Root-Verzeichnis (`/`) finden

## Projekt-Struktur

```
BumblebeDetectionIDF/
├── CMakeLists.txt                 # Root Build-Konfiguration
├── sdkconfig.defaults             # Default-Einstellungen
├── .gitignore                     # Git-Ignore für Build-Artefakte
├── README.md                      # Diese Datei
│
├── main/
│   ├── CMakeLists.txt             # Component Build-Config
│   ├── idf_component.yml          # Dependency Management
│   ├── app_main.cpp               # Hauptprogramm (~460 Zeilen)
│   │
│   ├── include/
│   │   ├── camera_pins.h          # Kamera Pin-Definitionen
│   │   ├── sd_pins.h              # SD-Karte Pin-Definitionen
│   │   └── sd_card.hpp            # SD-Karte API Interface
│   │
│   ├── src/
│   │   └── sd_card.cpp            # SD-Karte Implementierung
│   │
│   └── models/
│       └── yolov8_bumblebee_quantized.espdl  # Quantisiertes Modell (~2MB)
│
└── build/                         # Build-Output (ignoriert)
    └── bumblebee-detection.bin    # Finales Binary
```
