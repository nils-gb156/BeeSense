from ultralytics import YOLO

# Modell laden
model = YOLO('yolov8n.pt')

# Training starten
model.train(
    data='yolov8_bumblebee.yaml',
    imgsz=[640, 480],
    epochs=100,
    batch=16,
    cache=True
)
