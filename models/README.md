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

## 4. Weiteres Vorgehen

- Nach erfolgreichem Test auf PC kannst du das Modell quantisieren und auf ESP32-S3 deployen
- Zone-based Counting implementieren:
  - Zonen definieren (z.B. Ein- / Ausflug)
  - Tracker-ID nutzen, um einzelne Bienen zu zählen
- Ergebnisse über MQTT / SD-Karte speichern und Dashboard darstellen

---

## 5. Quelle der Trainingsdaten
Eigene Aufnahmen