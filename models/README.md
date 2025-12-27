# Models

## Training

Trainiere das Modell mit YOLOv8n:

```python
python models/train.py
```

Nach dem Training findest du die Gewichte in: `models/runs/detect/train/weights/best.pt`

---

## 3. Testen

### 3.1 Einzelbild

Du kannst ein Testbild mit `models/test.py` prüfen:

- Passe den Pfad zum Testbild an.
- Annotierte Bilder werden in `models/runs/detect/predict/` gespeichert.
- `conf` = Confidence Threshold, kann angepasst werden (z.B. 0.2–0.5).

### 3.2 Video mit Tracking

Für ein Video mit Tracking (BoTSORT):

```
yolo track model=models/runs/detect/train/weights/best.pt source=data/videos/bee_video.mp4 tracker=botsort.yaml conf=0.3 save=True
```

- Jede Biene erhält eine ID, die über Frames verfolgt wird
- Perfekt für Zone-based Counting
- Output-Video wird automatisch in `runs/track/` gespeichert

---

## 4. ESP32-S3 Deployment

Nach dem Training kann das Modell für ESP32-S3 quantisiert und deployed werden.

### 4.1 Schnellanleitung für neues Modell

Wenn du ein neues Modell trainiert hast (`best.pt`), führe folgende Schritte aus:

```bash
cd models

# 1. Exportiere zu ONNX (QVGA für beste Performance)
python export_onnx.py --model runs/detect/train/weights/best.pt --output runs/detect/train/weights/best.onnx --img-size 320 256

# 2. Erstelle Kalibrierungsdaten (100 Samples)
python prepare_calib_data.py --data-dir ../data --output-dir ./calib_data --img-size 320 256 --num-samples 100

# 3. Quantisiere für ESP32-S3
python quantize_yolov8.py --onnx runs/detect/train/weights/best.onnx --calib-dir ./calib_data --output-dir ./quantized_model_qvga --target esp32s3

# 4. Kopiere quantisiertes Modell ins ESP32-Projekt
Copy-Item quantized_model_qvga/yolov8_bumblebee_quantized.espdl ` -Destination ../hardware/firmware/esp32cam/BumblebeeDetection/main/models/

# 5. Räume temporäre Dateien auf
Remove-Item -Recurse -Force calib_data
Remove-Item runs/detect/train/weights/best.onnx
```

**Hinweis:** Die Höhe wird automatisch von 240 auf 256 angepasst (YOLOv8 benötigt Vielfache von 32).

### 4.2 Modellkonfiguration

- **Auflösung:** 320x256 (QVGA optimiert)
- **Quantisierung:** 8-bit symmetrisch + POWER OF TWO
- **Target:** ESP32-S3 mit PSRAM
- **Erwartete Performance:** 4-8 FPS, 100-250ms Inferenzzeit

### 4.3 ESP-IDF Setup

Detaillierte Anweisungen siehe `ESP32_DEPLOYMENT.md`. Kurzfassung:

```bash
cd ../hardware/firmware/esp32cam/BumblebeDetection

# ESP-DL Library hinzufügen
idf.py add-dependency "espressif/esp-dl"

# Bauen und Flashen
idf.py set-target esp32s3
idf.py menuconfig  # PSRAM aktivieren
idf.py build
idf.py -p COM<X> flash monitor
```

### 4.4 Benötigte Dateien

**Behalten für Re-Training:**
- `best.pt` - Trainiertes Modell
- `export_onnx.py`, `prepare_calib_data.py`, `quantize_yolov8.py` - Deployment-Skripte
- `quantized_model_qvga/*.espdl` - Quantisiertes Modell für ESP32

**Temporär (werden nach Quantisierung gelöscht):**
- `calib_data/` - Kalibrierungsdaten
- `best.onnx` - ONNX-Zwischendatei

---

## 5. Weiteres Vorgehen

- Zone-based Counting implementieren:
  - Zonen definieren (z.B. Ein- / Ausflug)
  - Tracker-ID nutzen, um einzelne Bienen zu zählen
- Ergebnisse über MQTT / SD-Karte speichern und Dashboard darstellen
- Modell kontinuierlich mit neuen Daten verbessern

---

## 6. Quelle der Trainingsdaten
Eigene Aufnahmen