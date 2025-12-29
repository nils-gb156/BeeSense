import os
from glob import glob
from PIL import Image

# Passe die Pfade ggf. an
base_dir = "../data/images"
subdirs = ["test"]
target_size = 96

for sub in subdirs:
    folder = os.path.join(base_dir, sub)
    if not os.path.isdir(folder):
        continue
    for img_path in glob(os.path.join(folder, "*")):
        try:
            with Image.open(img_path) as img:
                img = img.convert("RGB")
                img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
                img.save(img_path)
                print(f"Resized to {target_size}x{target_size}: {img_path}")
        except Exception as e:
            print(f"Error with {img_path}: {e}")