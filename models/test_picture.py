from ultralytics import YOLO

# Modell laden
model = YOLO("runs/detect/train/weights/best.pt")

# Inferenz auf einem Testbild
results = model.predict(
    source="../data/images/test/20251202165507_jpg.rf.cdf6cfa37e4fe23fe5ba09e6b99408c2.jpg",  # Pfad zum Testbild
    conf=0.3,   # Confidence Threshold, z.B. 0.3
    save=True,  # speichert Bild mit Bounding Boxes
    show=True   # öffnet das Bild im Fenster
)

# Ergebnisse ausgeben
for r in results:
    print("Gefundene Hummeln:", len(r.boxes))
    print("Koordinaten:", r.boxes.xyxy)  # xmin, ymin, xmax, ymax
