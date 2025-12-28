from ultralytics import YOLO

# Modell laden
model = YOLO('yolo11n.pt')

# Training starten
model.train(
    data='yolov11_bumblebee.yaml',
    imgsz=[96, 96],
    epochs=50,
    batch=16,
    cache=True
)
