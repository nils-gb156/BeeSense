#pragma once

#include <cstdint>
#include <cstddef>

namespace sdcard {

bool init();

bool create_dir(const char *full_path);

int count_files(const char *full_path);

// Save RGB888 image as JPEG
bool save_jpeg_with_timestamp(const uint8_t *rgb888_data, int width, int height, const char *dir_full_path);

// Save RGB888 image as JPEG with detection count in filename
bool save_jpeg_with_detections(const uint8_t *rgb888_data, int width, int height, int detection_count, const char *dir_full_path);

} // namespace sdcard
