from ultralytics import YOLO

# Modell laden
model = YOLO("runs/detect/train/weights/best.pt")

# Inferenz auf einem Testbild
results = model.predict(
    source="../data/images/test/20251202165240_jpg.rf.3de78bfa6beba2fc22728c0d5225cfe3.jpg",  # Pfad zum Testbild
    conf=0.1,   # Confidence Threshold, z.B. 0.3
    save=True,  # speichert Bild mit Bounding Boxes
    show=True   # öffnet das Bild im Fenster
)

# Ergebnisse ausgeben
for r in results:
    print("Gefundene Hummeln:", len(r.boxes))
    print("Koordinaten:", r.boxes.xyxy)  # xmin, ymin, xmax, ymax
