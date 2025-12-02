# BeeSense
Automatisierte Erfassung von Bienenflügen mit der SenseBox Eye

## Ordnerstruktur

```
BeeSense/
├── docs/                # Projekt-Dokumentation, Pitch, Präsentationen, Poster
│   ├── pitch/           # z.B. beeSense_pitch.pdf
│   ├── reports/         # Zwischenberichte, Abschlussbericht
│   └── diagrams/        # Architektur- oder Ablaufdiagramme
│
├── hardware/            # Mikrocontroller, Sensoren, Schaltpläne
│   ├── schematics/      # Schaltpläne, Verdrahtung
│   ├── firmware/        # ESP32 Code (Arduino/PlatformIO)
│   └── 3d_print/        # STL-Dateien für Gehäuse
│
├── data/                # Rohdaten & Annotationen
│   ├── raw/             # Originalbilder/Videos
│   ├── labeled/         # Labels im YOLO/COCO Format
│   └── processed/       # Vorverarbeitete Daten (z.B. train/test split)
│
├── models/              # KI-Modelle
│   ├── training/        # Trainingsskripte (YOLOv8, Edge Impulse)
│   ├── weights/         # Gespeicherte Modelle (.pt, .onnx)
│   └── evaluation/      # Ergebnisse, Metriken, Confusion-Matrix
│
├── backend/             # Datenbank & API
│   ├── sql/             # Init-Skripte, Tabellen-Definitionen
│   ├── api/             # REST/GraphQL Schnittstelle
│   └── config/          # Konfigurationsdateien (Docker, Env)
│
├── dashboard/           # Frontend
│   ├── src/             # React/Vue/Angular Code
│   ├── public/          # Assets
│   └── styles/          # CSS/Theme
│
├── tests/               # Unit- und Integrationstests
│
├── scripts/             # Hilfsskripte (z.B. Datenimport, Automatisierung)
│
├── .gitignore
├── README.md            # Projektübersicht
└── LICENSE              # Lizenz (z.B. MIT)
```
