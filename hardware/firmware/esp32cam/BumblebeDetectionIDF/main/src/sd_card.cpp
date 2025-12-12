#include "sd_card.hpp"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"

#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <time.h>
#include <errno.h>

#include "img_converters.h"
#include "include/sd_pins.h"

namespace sdcard {

static const char *TAG = "SDCARD";
static constexpr const char *MOUNT_POINT = "/sdcard";

static sdmmc_card_t *g_card = nullptr;
static bool g_mounted = false;

// --------- Internal helpers ----------------------------------

static void init_sd_enable_pin(void) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << SD_ENABLE);
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(SD_ENABLE, 0); // Active low
}

static bool mount_sdcard_spi() {
    if (g_mounted) {
        return true;
    }

    init_sd_enable_pin();
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Auto-format if needed
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false
    };

    ESP_LOGI(TAG, "Initializing SD card over SPI");

    constexpr spi_host_device_t SPI_HOST_ID = SPI3_HOST;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 5000;
    host.slot = SPI_HOST_ID;

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num      = PIN_NUM_MOSI;
    bus_cfg.miso_io_num      = PIN_NUM_MISO;
    bus_cfg.sclk_io_num      = PIN_NUM_CLK;
    bus_cfg.quadwp_io_num    = -1;
    bus_cfg.quadhd_io_num    = -1;
    bus_cfg.max_transfer_sz  = 4000;

    ESP_LOGI(TAG, "Initializing SPI bus");
    ret = spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }

    gpio_reset_pin(PIN_NUM_CS);
    gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_CS, 1);

    sdspi_device_config_t slot_config = {};
    slot_config.host_id      = SPI_HOST_ID;
    slot_config.gpio_cs      = PIN_NUM_CS;
    slot_config.gpio_cd      = SD_SW;
    slot_config.gpio_wp      = SDSPI_SLOT_NO_WP;
    slot_config.gpio_int     = GPIO_NUM_NC;
    slot_config.gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW;

    ESP_LOGI(TAG, "Mounting FAT filesystem at %s", MOUNT_POINT);
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &g_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SD", "Failed to mount filesystem");
        } else {
            ESP_LOGE("SD", "Failed to initialize card: %s", esp_err_to_name(ret));
        }
        return false;
    }

    sdmmc_card_print_info(stdout, g_card);
    g_mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully");
    return true;
}

static bool encode_img_to_jpeg(const uint8_t *rgb888_data, int width, int height, uint8_t **out_buf, size_t *out_len) {
    // Convert RGB888 to JPEG using img_converters
    return fmt2jpg(const_cast<uint8_t*>(rgb888_data), 
                   width * height * 3,
                   width, 
                   height, 
                   PIXFORMAT_RGB888,
                   80,  // Quality
                   out_buf, 
                   out_len);
}

static char* get_timestamp() {
    static char buf[32];
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    return buf;
}

// --------- Public API ----------------------------------

bool init() {
    return mount_sdcard_spi();
}

bool create_dir(const char *full_path) {
    if (!g_mounted) {
        ESP_LOGE(TAG, "create_dir: SD not mounted");
        return false;
    }

    struct stat st;
    if (stat(full_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            ESP_LOGI(TAG, "Dir already exists: %s", full_path);
            return true;
        } else {
            ESP_LOGE(TAG, "Path exists but is not a directory: %s", full_path);
            return false;
        }
    }

    int res = mkdir(full_path, 0775);
    if (res == 0) {
        ESP_LOGI(TAG, "Created dir: %s", full_path);
        return true;
    } else {
        ESP_LOGE(TAG, "mkdir failed for %s (errno=%d)", full_path, errno);
        return false;
    }
}

int count_files(const char *path) {
    int count = 0;
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE("FILE_COUNT", "Failed to open directory: %s", path);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[256];
        strlcpy(full_path, path, sizeof(full_path));
        strlcat(full_path, "/", sizeof(full_path));
        strlcat(full_path, entry->d_name, sizeof(full_path));
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                count++;
            }
        }
    }

    closedir(dir);
    return count;
}

bool save_jpeg_with_timestamp(const uint8_t *rgb888_data, int width, int height, const char *dir_full_path) {
    if (!g_mounted) {
        ESP_LOGE(TAG, "save_jpeg: SD not mounted");
        return false;
    }
    if (!rgb888_data || width <= 0 || height <= 0) {
        ESP_LOGE(TAG, "save_jpeg: invalid image");
        return false;
    }

    // TEST: Skip create_dir and write to root
    // if (!create_dir(dir_full_path)) {
    //     return false;
    // }

    uint8_t *jpeg_buf = nullptr;
    size_t jpeg_len = 0;

    if (!encode_img_to_jpeg(rgb888_data, width, height, &jpeg_buf, &jpeg_len)) {
        ESP_LOGE(TAG, "JPEG encoding failed");
        return false;
    }

    // Use simple counter-based filename instead of timestamp
    static int photo_counter = 0;
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/photo%03d.jpg", photo_counter++);

    ESP_LOGI(TAG, "Saving JPEG: %s", filepath);

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d (%s)", filepath, errno, strerror(errno));
        free(jpeg_buf);
        return false;
    }

    size_t written = fwrite(jpeg_buf, 1, jpeg_len, f);
    fclose(f);
    free(jpeg_buf);

    if (written != jpeg_len) {
        ESP_LOGE(TAG, "Failed to write complete file");
        return false;
    }

    ESP_LOGI(TAG, "Saved successfully (%zu bytes)", jpeg_len);
    return true;
}

bool save_jpeg_with_detections(const uint8_t *rgb888_data, int width, int height, int detection_count, const char *dir_full_path) {
    if (!g_mounted) {
        ESP_LOGE(TAG, "save_jpeg: SD not mounted");
        return false;
    }
    if (!rgb888_data || width <= 0 || height <= 0) {
        ESP_LOGE(TAG, "save_jpeg: invalid image");
        return false;
    }

    // TEST: Skip create_dir and write to root
    // if (!create_dir(dir_full_path)) {
    //     return false;
    // }

    uint8_t *jpeg_buf = nullptr;
    size_t jpeg_len = 0;

    if (!encode_img_to_jpeg(rgb888_data, width, height, &jpeg_buf, &jpeg_len)) {
        ESP_LOGE(TAG, "JPEG encoding failed");
        return false;
    }

    // Use simple counter-based filename
    static int detect_counter = 0;
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/detect%03d_n%d.jpg", detect_counter++, detection_count);  // TEST: Direct to root

    ESP_LOGI(TAG, "Saving JPEG with %d detections: %s", detection_count, filepath);

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        free(jpeg_buf);
        return false;
    }

    size_t written = fwrite(jpeg_buf, 1, jpeg_len, f);
    fclose(f);
    free(jpeg_buf);

    if (written != jpeg_len) {
        ESP_LOGE(TAG, "Failed to write complete file");
        return false;
    }

    ESP_LOGI(TAG, "Saved successfully (%zu bytes)", jpeg_len);
    return true;
}

} // namespace sdcard
