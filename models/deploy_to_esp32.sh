#!/bin/bash

# Quick deployment script for YOLOv8 to ESP32-S3
# Run this script from the models/ directory

set -e  # Exit on error

echo "========================================"
echo "YOLOv8 to ESP32-S3 Deployment Pipeline"
echo "========================================"
echo ""

# Configuration
MODEL_PATH=${1:-"runs/detect/train/weights/best.pt"}
IMG_WIDTH=${2:-640}
IMG_HEIGHT=${3:-480}
CALIB_SAMPLES=${4:-100}

echo "Configuration:"
echo "  Model: $MODEL_PATH"
echo "  Image size: ${IMG_WIDTH}x${IMG_HEIGHT}"
echo "  Calibration samples: $CALIB_SAMPLES"
echo ""

# Step 1: Export to ONNX
echo "[1/4] Exporting YOLOv8 to ONNX..."
python export_onnx.py \
    --model "$MODEL_PATH" \
    --img-size $IMG_WIDTH $IMG_HEIGHT \
    || { echo "❌ ONNX export failed"; exit 1; }
echo "✓ ONNX export complete"
echo ""

# Step 2: Prepare calibration data
echo "[2/4] Preparing calibration data..."
python prepare_calib_data.py \
    --data-dir ../data \
    --output-dir ./calib_data \
    --img-size $IMG_WIDTH $IMG_HEIGHT \
    --num-samples $CALIB_SAMPLES \
    || { echo "❌ Calibration data preparation failed"; exit 1; }
echo "✓ Calibration data ready"
echo ""

# Step 3: Quantize model
echo "[3/4] Quantizing model with ESP-PPQ..."
echo "This may take several minutes..."
python quantize_yolov8.py \
    --onnx yolov8_bumblebee.onnx \
    --calib-dir ./calib_data \
    --output-dir ./quantized_model \
    --target esp32s3 \
    --bits 8 \
    || { echo "❌ Quantization failed"; exit 1; }
echo "✓ Model quantization complete"
echo ""

# Step 4: Copy to ESP32 project
echo "[4/4] Copying model to ESP32 project..."
mkdir -p ../hardware/firmware/esp32cam/BumblebeDetection/main/model
cp quantized_model/yolov8_bumblebee_quantized.espdl \
   ../hardware/firmware/esp32cam/BumblebeDetection/main/model/ \
   || { echo "❌ Failed to copy model"; exit 1; }
echo "✓ Model copied to ESP32 project"
echo ""

# Summary
echo "========================================"
echo "✓ Deployment pipeline complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. cd ../hardware/firmware/esp32cam/BumblebeDetection"
echo "2. idf.py set-target esp32s3"
echo "3. idf.py menuconfig  # Enable PSRAM and configure camera"
echo "4. idf.py build"
echo "5. idf.py -p /dev/ttyUSB0 flash monitor"
echo ""
echo "Check quantized_model/yolov8_bumblebee_quantized.info"
echo "for quantization error analysis."
echo ""
