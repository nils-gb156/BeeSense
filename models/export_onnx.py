import torch.nn as nn

# Hilfsfunktion: Ersetze Swish/SiLU durch SiLU (oder ReLU, falls gewünscht)
def replace_swish_with_silu(model):
    for name, module in model.named_modules():
        # Ersetze als Submodul (nur SiLU, Swish gibt es in torch nicht)
        if isinstance(module, nn.SiLU):
            parent = model
            *path, last = name.split('.')
            for p in path:
                parent = getattr(parent, p)
            setattr(parent, last, nn.SiLU())  # oder nn.ReLU()
        # Ersetze als Attribut
        if hasattr(module, 'act') and module.act is not None:
            if module.act.__class__.__name__.lower() in ['silu', 'swish']:
                module.act = nn.SiLU()  # oder nn.ReLU()
    return model
"""
Export YOLOv8 model to ONNX format compatible with ESP-DL
Based on Espressif's ESP-DL YOLO11n deployment guide
"""

import torch
from ultralytics import YOLO
from ultralytics.nn.modules import Detect
import argparse


class DetectESPDL(Detect):
    """
    Custom Detect head for ESP-DL deployment.
    Separates box and class score outputs instead of concatenating them.
    This reduces quantization errors and improves inference speed.
    """
    def forward(self, x):
        boxes = []
        scores = []
        
        for i, layer in enumerate(x):
            # Get predictions from each detection head
            pred = self.cv2[i](layer)  # Box predictions
            score = self.cv3[i](layer)  # Class predictions
            
            # Reshape and store
            b, _, h, w = pred.shape
            boxes.append(pred.view(b, self.reg_max * 4, -1))
            scores.append(score.view(b, self.nc, -1))
        
        # Concatenate along spatial dimension
        boxes = torch.cat(boxes, dim=2)
        scores = torch.cat(scores, dim=2)
        
        return boxes, scores


def export_yolov8_to_onnx(
    model_path: str,
    output_path: str = None,
    img_size: tuple = (96, 96),
    opset_version: int = 11,
    simplify: bool = True
):
    """
    Export YOLOv8 model to ONNX format compatible with ESP-DL
    
    Args:
        model_path: Path to YOLOv8 .pt model file
        output_path: Path for output ONNX file (default: same as model_path with .onnx extension)
        img_size: Input image size as (width, height)
        opset_version: ONNX opset version
        simplify: Whether to simplify the ONNX model
    """
    print(f"Loading YOLOv8 model from: {model_path}")
    model = YOLO(model_path)

    # Ersetze Swish/SiLU durch SiLU (oder ReLU)
    print("Ersetze Swish/SiLU durch SiLU für ESP-DL Kompatibilität...")
    model.model = replace_swish_with_silu(model.model)

    # Replace the Detect head with ESP-DL compatible version
    print("Replacing Detect head with ESP-DL compatible version...")
    for module in model.model.modules():
        if isinstance(module, Detect):
            module.__class__ = DetectESPDL
            print(f"✓ Replaced Detect head - Channels: {module.nc}, Reg_max: {module.reg_max}")
    
    # Set output path
    if output_path is None:
        output_path = model_path.replace('.pt', '.onnx')
    
    print(f"\nExporting to ONNX format...")
    print(f"  Image size: {img_size}")
    print(f"  Opset version: {opset_version}")
    print(f"  Output path: {output_path}")
    
    # Export model
    model.export(
        format='onnx',
        imgsz=img_size,
        opset=opset_version,
        simplify=simplify,
        dynamic=False,  # ESP-DL requires static shapes
    )
    
    print(f"\n✓ Successfully exported ONNX model to: {output_path}")
    print("\nNext steps:")
    print("1. Prepare calibration dataset with prepare_calib_data.py")
    print("2. Quantize the model with quantize_yolov8.py")
    print("3. Deploy to ESP32-S3 using ESP-DL")
    
    return output_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Export YOLOv8 to ONNX for ESP-DL')
    parser.add_argument(
        '--model',
        type=str,
        default='yolov8n.pt',
        help='Path to YOLOv8 .pt model file'
    )
    parser.add_argument(
        '--output',
        type=str,
        default=None,
        help='Output ONNX file path (default: same as model with .onnx extension)'
    )
    parser.add_argument(
        '--img-size',
        nargs=2,
        type=int,
        default=[96, 96],
        help='Input image size als Breite Höhe (Standard: 96 96)'
    )
    parser.add_argument(
        '--opset',
        type=int,
        default=11,
        help='ONNX opset version (default: 11)'
    )
    parser.add_argument(
        '--no-simplify',
        action='store_true',
        help='Do not simplify ONNX model'
    )
    
    args = parser.parse_args()
    
    export_yolov8_to_onnx(
        model_path=args.model,
        output_path=args.output,
        img_size=tuple(args.img_size),
        opset_version=args.opset,
        simplify=not args.no_simplify
    )
