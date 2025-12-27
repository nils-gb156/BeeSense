"""
Quantize YOLOv8 ONNX model using ESP-PPQ for ESP32-S3 deployment
Based on Espressif's ESP-DL quantization guide
"""

import os
import numpy as np
from pathlib import Path
import argparse
from glob import glob


def load_calibration_data(calib_dir: str, batch_size: int = 32):
    """
    Load calibration data from directory
    
    Args:
        calib_dir: Directory containing .npy calibration files
        batch_size: Batch size for quantization
        
    Returns:
        List of calibration data batches
    """
    calib_files = sorted(glob(os.path.join(calib_dir, "*.npy")))
    
    if len(calib_files) == 0:
        raise ValueError(f"No calibration files found in {calib_dir}")
    
    print(f"Loading {len(calib_files)} calibration samples...")
    
    calibration_data = []
    for i in range(0, len(calib_files), batch_size):
        batch_files = calib_files[i:i+batch_size]
        batch = []
        
        for file in batch_files:
            data = np.load(file)
            # Add batch dimension and convert to NCHW format
            data = np.transpose(data, (2, 0, 1))  # HWC -> CHW
            data = np.expand_dims(data, axis=0)   # Add batch dimension
            batch.append(data)
        
        if len(batch) == batch_size:
            batch_data = np.concatenate(batch, axis=0)
            calibration_data.append(batch_data)
    
    print(f"Created {len(calibration_data)} calibration batches")
    return calibration_data


def replace_swish_with_relu(onnx_path, output_path=None):
    """
    Ersetzt alle Swish/SiLU-Aktivierungen im ONNX-Modell durch ReLU.
    Speichert das neue Modell unter output_path (oder überschreibt onnx_path).
    """
    import onnx
    from onnx import helper
    model = onnx.load(onnx_path)
    replaced = 0
    for node in model.graph.node:
        # Ersetze SiLU/Swish direkt
        if node.op_type in ["Swish", "SiLU"]:
            node.op_type = "Relu"
            node.input[:] = node.input[:1]  # ReLU hat nur 1 Input
            replaced += 1
        # Ersetze zusammengesetzte Swish (Mul mit Sigmoid)
    # Suche nach Mul-Knoten, deren Inputs ein Sigmoid ist
    for node in model.graph.node:
        if node.op_type == "Mul":
            sig_in = None
            for inp in node.input:
                for n2 in model.graph.node:
                    if n2.output and inp == n2.output[0] and n2.op_type == "Sigmoid":
                        sig_in = inp
            if sig_in:
                # Ersetze Mul durch Relu
                node.op_type = "Relu"
                # Nimm den anderen Input (das "x" aus x*sigmoid(x))
                other_in = [i for i in node.input if i != sig_in][0]
                node.input[:] = [other_in]
                replaced += 1
    if replaced > 0:
        onnx.save(model, output_path or onnx_path)
        print(f"[INFO] Ersetzte {replaced} Swish/SiLU durch ReLU im ONNX-Modell.")
    else:
        print("[INFO] Keine Swish/SiLU-Knoten im ONNX-Modell gefunden.")

def quantize_yolov8(
    onnx_model_path: str,
    calib_dir: str,
    output_dir: str = "./quantized_model",
    target: str = "esp32s3",
    num_of_bits: int = 8,
    batch_size: int = 32,
    input_shape: tuple = None
):
    """
    Quantize YOLOv8 ONNX model using ESP-PPQ
    
    Args:
        onnx_model_path: Path to ONNX model
        calib_dir: Directory containing calibration data
        output_dir: Output directory for quantized model
        target: Target platform (esp32s3, esp32p4, esp32)
        num_of_bits: Quantization bits (8 or 16)
        batch_size: Batch size for calibration
        input_shape: Input shape (B, C, H, W) - if None, will be inferred
    """
    
    try:
        from esp_ppq import QuantizationSettingFactory, TargetPlatform
        from esp_ppq.api import espdl_quantize_onnx
    except ImportError:
        print("ERROR: ESP-PPQ not installed!")
        print("\nPlease install ESP-PPQ first:")
        print("  git clone https://github.com/espressif/esp-ppq.git")
        print("  cd esp-ppq")
        print("  pip install -e .")
        return
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    print("\n" + "="*60)
    print("YOLOv8 Model Quantization for ESP32-S3")
    print("="*60)
    print(f"ONNX Model: {onnx_model_path}")
    print(f"Calibration data: {calib_dir}")
    print(f"Target platform: {target}")
    print(f"Quantization bits: {num_of_bits}")
    print(f"Batch size: {batch_size}")
    print(f"Output directory: {output_dir}")
    print("="*60 + "\n")
    
    # Load calibration data
    calibration_data = load_calibration_data(calib_dir, batch_size)
    
    if len(calibration_data) == 0:
        raise ValueError("No calibration data loaded!")
    
    # Determine input shape
    if input_shape is None:
        # Infer from calibration data
        input_shape = calibration_data[0].shape
        print(f"Inferred input shape: {input_shape}")
    
    # Set up quantization settings
    print("\nSetting up quantization configuration...")
    quant_setting = QuantizationSettingFactory.espdl_setting()
    
    # Optional: Mixed precision quantization for better accuracy
    # Uncomment the following lines if you want to use 16-bit for certain layers
    # This can improve accuracy at the cost of slightly larger model size
    """
    if num_of_bits == 8:
        print("Applying mixed-precision optimization...")
        from esp_ppq.api import get_target_platform
        # Quantize specific layers with 16-bits for better accuracy
        # These are typically the first few layers
        quant_setting.dispatching_table.append("/model.0/conv/Conv", get_target_platform(target, 16))
        quant_setting.dispatching_table.append("/model.1/conv/Conv", get_target_platform(target, 16))
    """
    
    # Swish/SiLU durch ReLU ersetzen (ONNX wird überschrieben)
    replace_swish_with_relu(onnx_model_path)

    # Perform quantization
    print("\nStarting quantization process...")
    print("This may take several minutes...\n")
    try:
        import torch
        # Create collate function
        def collate_fn(batch):
            if isinstance(batch, np.ndarray):
                return torch.from_numpy(batch).float()
            return batch
        espdl_quantize_onnx(
            onnx_import_file=onnx_model_path,
            espdl_export_file=os.path.join(output_dir, "yolov8_bumblebee_quantized.espdl"),
            calib_dataloader=calibration_data,
            calib_steps=len(calibration_data),
            input_shape=input_shape,
            target=target,
            num_of_bits=num_of_bits,
            setting=quant_setting,
            collate_fn=collate_fn,
            # Export test values disabled due to large array printing issue
            export_test_values=False
        )
        print("\n" + "="*60)
        print("✓ Quantization completed successfully!")
        print("="*60)
        print(f"\nOutput files in {output_dir}:")
        print("  - yolov8_bumblebee_quantized.espdl  (Binary model for ESP32)")
        print("  - yolov8_bumblebee_quantized.info   (Model information & debug)")
        print("  - yolov8_bumblebee_quantized.json   (Quantization parameters)")
        print("\nNext steps:")
        print("1. Copy the .espdl file to your ESP32 project")
        print("2. Use ESP-DL to load and run the model")
        print("3. Check the .info file for model structure and test values")
    except Exception as e:
        print(f"\n❌ Quantization failed: {e}")
        print("\nTroubleshooting:")
        print("1. Check that ONNX model is valid")
        print("2. Verify calibration data format")
        print("3. Ensure ESP-PPQ is properly installed")
        raise


def validate_quantized_model(
    quantized_model_path: str,
    test_input: np.ndarray = None
):
    """
    Validate the quantized model (optional)
    
    Args:
        quantized_model_path: Path to quantized .espdl model
        test_input: Test input data
    """
    print("\nValidation is best done on the actual ESP32 device")
    print("Check the .info file for test input/output values")
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Quantize YOLOv8 for ESP32-S3')
    parser.add_argument(
        '--onnx',
        type=str,
        default='yolov8n.onnx',
        help='Path to ONNX model file'
    )
    parser.add_argument(
        '--calib-dir',
        type=str,
        default='./calib_data',
        help='Directory containing calibration data'
    )
    parser.add_argument(
        '--output-dir',
        type=str,
        default='./quantized_model',
        help='Output directory for quantized model'
    )
    parser.add_argument(
        '--target',
        type=str,
        default='esp32s3',
        choices=['esp32', 'esp32s3', 'esp32p4'],
        help='Target ESP32 platform (default: esp32s3)'
    )
    parser.add_argument(
        '--bits',
        type=int,
        default=8,
        choices=[8, 16],
        help='Quantization bits (default: 8)'
    )
    parser.add_argument(
        '--batch-size',
        type=int,
        default=32,
        help='Calibration batch size (default: 32)'
    )
    
    args = parser.parse_args()
    
    quantize_yolov8(
        onnx_model_path=args.onnx,
        calib_dir=args.calib_dir,
        output_dir=args.output_dir,
        target=args.target,
        num_of_bits=args.bits,
        batch_size=args.batch_size
    )
