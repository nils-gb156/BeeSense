import os
from glob import glob
from PIL import Image

# Passe die Pfade ggf. an
base_dir = "../data/images"
subdirs = ["train", "val", "test"]
target_size = 320

for sub in subdirs:
    folder = os.path.join(base_dir, sub)
    if not os.path.isdir(folder):
        continue
    for img_path in glob(os.path.join(folder, "*")):
        try:
            with Image.open(img_path) as img:
                img = img.convert("RGB")
                w_percent = 320 / float(img.width)
                h_size = int(float(img.height) * w_percent)
                img = img.resize((320, h_size), Image.Resampling.LANCZOS)
                img.save(img_path)
                print(f"Resized: {img_path}")
        except Exception as e:
            print(f"Error with {img_path}: {e}")