# Models

## 1. Training

Trainiere das Modell mit YOLOv8n:

```python
python models/train.py
```

Nach dem Training findest du die Gewichte in: `models/runs/detect/train/weights/best.pt`

---

## 2. Testen

Du kannst Bilder aus `data/images/test` mit `models/predict_pictures.py` prüfen:

- Annotierte Bilder werden in `models/runs/detect/predict/` gespeichert.
- `conf` = Confidence Threshold, kann angepasst werden (z.B. 0.2–0.5).


## 3. ESP32-S3 Deployment

Nach dem Training kann das Modell für ESP32-S3 quantisiert und deployed werden.

Wenn du ein neues Modell trainiert hast (`best.pt`), führe folgende Schritte aus:

```bash
cd models

# 1. Exportiere zu ONNX
python export_onnx.py

# 2. Erstelle Kalibrierungsdaten
python prepare_calib_data.py

# 3. Quantisiere für ESP32-S3
python quantize_onnx_model.py
```

Das quantisierte esp-dl Model findest du im Ordner `quantized_model`.
