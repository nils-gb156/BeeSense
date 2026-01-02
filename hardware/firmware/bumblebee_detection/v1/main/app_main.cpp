
#include "esp_imgfx_crop.h"
#include "dl_image.hpp"
#define MODEL_IMG_SIZE 224
#include <stdio.h>
#include <algorithm>
#include "bumblebee_detect.hpp"
#include "esp_camera.h"
#include "esp_log.h"
#include "sd_card.hpp"
#include <esp_system.h>
#include <string.h>
#include <vector>
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/camera_pins.h"
#include "dl_image_draw.hpp"
#include "dl_image_color.hpp"

extern const uint8_t bumblebee_jpg_start[] asm("_binary_bumblebee_jpg_start");
extern const uint8_t bumblebee_jpg_end[] asm("_binary_bumblebee_jpg_end");
const char *TAG = "bumblebee_detect";

// Camera Module pin mapping
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,

    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000, // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, // PIXFORMAT_RGB565 , PIXFORMAT_JPEG
    .frame_size = FRAMESIZE_QVGA, // [<<320x240>> (QVGA, 4:3); FRAMESIZE_320X320, 240x176 (HQVGA, 15:11); 400x296 (CIF,
                                  // 50:37)],FRAMESIZE_QVGA,FRAMESIZE_VGA

    .jpeg_quality = 8, // 0-63 lower number means higher quality.  Reduce quality if stack overflow in cam_task
    .fb_count = 2,     // if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port = 0 // optional
};

static esp_err_t init_camera(void)
{
    // Initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE("CAM", "Camera Init Failed");
    }
    return err;
}

// Hilfsfunktion: Bild aufnehmen, croppen und in RGB888 konvertieren
static bool capture_and_convert_image(dl::image::img_t &cropped_img) {
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE("CAM", "Failed to capture image");
        return false;
    }
    dl::image::img_t img;
    img.height = pic->height;
    img.width = pic->width;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565;
    img.data = malloc(pic->len);
    if (!img.data) {
        ESP_LOGE("MEM", "Memory allocation failed");
        esp_camera_fb_return(pic);
        return false;
    }

    memcpy(img.data, pic->buf, pic->len);
    esp_camera_fb_return(pic);

    // RGB565 -> RGB888 (ohne Crop)
    dl::image::img_t rgb888_img;
    rgb888_img.height = img.height;
    rgb888_img.width = img.width;
    rgb888_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    rgb888_img.data = malloc(img.width * img.height * 3);
    if (!rgb888_img.data) {
        ESP_LOGE("MEM", "Failed to allocate rgb888 buffer");
        free(img.data);
        return false;
    }
    dl::image::RGB5652RGB888<true, false> converter;
    uint8_t* src = (uint8_t*)img.data;
    uint8_t* dst = (uint8_t*)rgb888_img.data;
    for (int i = 0; i < img.width * img.height; ++i) {
        converter(src, dst);
        src += 2;
        dst += 3;
    }

    // Crop auf 224x224
    cropped_img.height = MODEL_IMG_SIZE;
    cropped_img.width = MODEL_IMG_SIZE;
    cropped_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    cropped_img.data = malloc(MODEL_IMG_SIZE * MODEL_IMG_SIZE * 3);
    if (!cropped_img.data) {
        ESP_LOGE("MEM", "Failed to allocate cropped buffer");
        free(img.data);
        free(rgb888_img.data);
        return false;
    }
    int x0 = (rgb888_img.width - MODEL_IMG_SIZE) / 2;
    int y0 = (rgb888_img.height - MODEL_IMG_SIZE) / 2;
    for (int y = 0; y < MODEL_IMG_SIZE; ++y) {
        int src_y = y0 + y;
        memcpy(
            (uint8_t*)cropped_img.data + y * MODEL_IMG_SIZE * 3,
            (uint8_t*)rgb888_img.data + (src_y * rgb888_img.width + x0) * 3,
            MODEL_IMG_SIZE * 3);
    }

    free(img.data);
    free(rgb888_img.data);
    return true;
}

extern "C" void app_main(void)
{
    ESP_LOGI("SD", "Mounting SD card...");
    bool mounted = sdcard::init();
    if (!mounted) {
        ESP_LOGE("SD", "SD card init/mount failed");
        return;
    }

    if (ESP_OK != init_camera()) {
        ESP_LOGE("APP", "Camera initialization failed");
        return;
    }

#if CONFIG_BUMBLEBEE_DETECT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif

    
    while (true) {
        ESP_LOGI("MEM", "Free heap at start of loop: %lu bytes", esp_get_free_heap_size());

        dl::image::img_t cropped_img;
        if (!capture_and_convert_image(cropped_img)) {
            ESP_LOGE("CAM", "Could not take or convert picture");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        BumblebeeDetect *detect = new BumblebeeDetect();
        auto &detect_results = detect->run(cropped_img);
        int result_count = 0;
        // BBoxen auf das 224x224 Bild zeichnen (gelb)
        for (const auto &res : detect_results) {
            if (res.category == 0 && res.score > 0.35f) {
                ESP_LOGI(TAG,
                         "[category: %d, score: %f, x1: %d, y1: %d, x2: %d, y2: %d]",
                         res.category,
                         res.score,
                         res.box[0],
                         res.box[1],
                         res.box[2],
                         res.box[3]);
                int x1 = res.box[0];
                int y1 = res.box[1];
                int x2 = res.box[2];
                int y2 = res.box[3];
                // Sortiere die Koordinaten, damit x1 < x2 und y1 < y2
                if (x2 < x1) std::swap(x1, x2);
                if (y2 < y1) std::swap(y1, y2);
                std::vector<uint8_t> color = {255, 0, 0}; // Rot
                dl::image::draw_hollow_rectangle(cropped_img, x1, y1, x2, y2, color, 2);
                ++result_count;
            }
        }
        if (result_count == 0) {
            ESP_LOGI(TAG, "Detection done, nothing detected (class 0 & score > 0.35)");
        } else {
            ESP_LOGI(TAG, "Detection done, results: %d", result_count);
        }
        // Bild mit BBoxen speichern
        dl::cls::result_t dummy_result = {};
        sdcard::save_detected_jpeg(cropped_img, dummy_result, "/sdcard/bumblebee_detect");
        delete detect;
        heap_caps_free(cropped_img.data);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

#if CONFIG_BUMBLEBEE_DETECT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_unmount());
#endif
}
