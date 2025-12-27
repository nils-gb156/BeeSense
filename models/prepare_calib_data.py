"""
Prepare calibration dataset for ESP-PPQ quantization
Converts training images to the format required by ESP-PPQ
"""

import os
import cv2
import numpy as np
from pathlib import Path
import argparse
from tqdm import tqdm


def prepare_calibration_data(
    data_dir: str,
    output_dir: str,
    img_size: tuple = (640, 480),
    num_samples: int = 100,
    subset: str = 'train'
):
    """
    Prepare calibration dataset from training images
    
    Args:
        data_dir: Root directory containing images (e.g., ../data)
        output_dir: Output directory for calibration data
        img_size: Target image size as (width, height)
        num_samples: Number of calibration samples to generate
        subset: Which subset to use ('train' or 'val')
    """
    
    # Setup paths
    data_path = Path(data_dir)
    images_path = data_path / 'images' / subset
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    print(f"Preparing calibration dataset...")
    print(f"  Source: {images_path}")
    print(f"  Output: {output_path}")
    print(f"  Image size: {img_size}")
    print(f"  Samples: {num_samples}")
    
    # Get all image files
    image_files = list(images_path.glob('*.jpg')) + list(images_path.glob('*.png'))
    
    if len(image_files) == 0:
        raise ValueError(f"No images found in {images_path}")
    
    print(f"  Found {len(image_files)} images")
    
    # Select samples
    if len(image_files) > num_samples:
        # Sample evenly across the dataset
        indices = np.linspace(0, len(image_files) - 1, num_samples, dtype=int)
        selected_files = [image_files[i] for i in indices]
    else:
        selected_files = image_files
        print(f"  Warning: Only {len(image_files)} images available, using all")
    
    # Process each image
    print("\nProcessing images...")
    for idx, img_file in enumerate(tqdm(selected_files)):
        try:
            # Read image
            img = cv2.imread(str(img_file))
            if img is None:
                print(f"  Warning: Could not read {img_file}, skipping")
                continue
            
            # Resize to target size
            img_resized = cv2.resize(img, img_size, interpolation=cv2.INTER_LINEAR)
            
            # Convert BGR to RGB
            img_rgb = cv2.cvtColor(img_resized, cv2.COLOR_BGR2RGB)
            
            # Normalize to [0, 1]
            img_normalized = img_rgb.astype(np.float32) / 255.0
            
            # Save as .npy file
            output_file = output_path / f"calib_{idx:04d}.npy"
            np.save(output_file, img_normalized)
            
        except Exception as e:
            print(f"  Error processing {img_file}: {e}")
            continue
    
    print(f"\n✓ Successfully created calibration dataset")
    print(f"  Output directory: {output_path}")
    print(f"  Total samples: {len(list(output_path.glob('*.npy')))}")
    
    # Create a metadata file
    metadata_file = output_path / "metadata.txt"
    with open(metadata_file, 'w') as f:
        f.write(f"Image size: {img_size}\n")
        f.write(f"Number of samples: {len(list(output_path.glob('*.npy')))}\n")
        f.write(f"Source: {images_path}\n")
        f.write(f"Format: RGB, normalized to [0, 1]\n")
    
    print(f"  Metadata saved to: {metadata_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Prepare calibration dataset for ESP-PPQ')
    parser.add_argument(
        '--data-dir',
        type=str,
        default='../data',
        help='Root directory containing images (default: ../data)'
    )
    parser.add_argument(
        '--output-dir',
        type=str,
        default='./calib_data',
        help='Output directory for calibration data (default: ./calib_data)'
    )
    parser.add_argument(
        '--img-size',
        nargs=2,
        type=int,
        default=[640, 480],
        help='Target image size as width height (default: 640 480)'
    )
    parser.add_argument(
        '--num-samples',
        type=int,
        default=100,
        help='Number of calibration samples (default: 100)'
    )
    parser.add_argument(
        '--subset',
        type=str,
        default='train',
        choices=['train', 'val'],
        help='Which dataset subset to use (default: train)'
    )
    
    args = parser.parse_args()
    
    prepare_calibration_data(
        data_dir=args.data_dir,
        output_dir=args.output_dir,
        img_size=tuple(args.img_size),
        num_samples=args.num_samples,
        subset=args.subset
    )
