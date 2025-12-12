#include "esp_log.h"
#include <esp_system.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"

#include "sd_card.hpp"
#include "include/camera_pins.h"

#include <time.h>

// Support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

// TODO: YOLOv8 detection will be added later
// extern const uint8_t model_espdl[] asm("_binary_yolov8_bumblebee_quantized_espdl_start");

static const char *TAG = "APP";

// WiFi credentials
#define WIFI_SSID "FRITZ!Box 7560 LC GB"
#define WIFI_PASS "48991416100471872244"

// HTTP server handles
static httpd_handle_t camera_httpd = NULL;

// Camera configuration
// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize WiFi
static void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, connecting to %s", WIFI_SSID);
}

// Initialize NTP
static void init_ntp() {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time sync
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

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

    .xclk_freq_hz = 10000000,  // Reduced from 20MHz to 10MHz for stability
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA, // 320x240

    .jpeg_quality = 12,
    .fb_count = 1,  // Reduced to 1 for stability
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,  // Changed from WHEN_EMPTY

    .sccb_i2c_port = 0
};

// Initialize camera
static esp_err_t init_camera(void) {
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
    }
    return err;
}

// TODO: YOLOv8 detection functions - will be added later
// For now, just return mock detection count
static int run_detection_mock() {
    ESP_LOGI(TAG, "Detection not yet implemented - returning mock count");
    return 0;
}

// ===== HTTP Handlers =====

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-S3 Bumblebee Detection</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #1a1a1a; color: #fff; margin: 0; padding: 20px; }
    h1 { color: #ffd700; margin-bottom: 30px; }
    #stream { width: 100%; max-width: 640px; border: 3px solid #ffd700; border-radius: 10px; margin: 20px auto; display: block; }
    button { background-color: #ffd700; color: #1a1a1a; border: none; padding: 15px 30px; font-size: 18px; font-weight: bold; border-radius: 5px; cursor: pointer; margin: 10px; transition: background-color 0.3s; }
    button:hover { background-color: #ffed4e; }
    button:active { background-color: #ccaa00; }
    .info { margin-top: 20px; color: #888; font-size: 14px; }
    #detection-result { color: #ffd700; font-size: 24px; margin-top: 20px; font-weight: bold; }
  </style>
</head>
<body>
  <h1>🐝 Bumblebee Detection</h1>
  <img id="stream" src="">
  <div>
    <button onclick="capturePhoto()">📷 Foto speichern</button>
    <button onclick="detectBees()">🔍 Hummeln erkennen</button>
  </div>
  <div class="info">
    <p>Auflösung: QVGA (320x240)</p>
    <p id="status"></p>
    <p id="detection-result"></p>
  </div>
  
  <script>
    document.getElementById('stream').src = 'http://' + window.location.hostname + ':81/stream';
    
    function capturePhoto() {
      const status = document.getElementById('status');
      status.textContent = 'Foto wird auf SD gespeichert...';
      
      fetch('/save')
        .then(response => response.text())
        .then(text => {
          status.textContent = text + ' ✓';
          setTimeout(() => status.textContent = '', 5000);
        })
        .catch(error => {
          status.textContent = 'Fehler: ' + error;
        });
    }
    
    function detectBees() {
      const result = document.getElementById('detection-result');
      const status = document.getElementById('status');
      
      status.textContent = 'Erkennung läuft...';
      result.textContent = '';
      
      fetch('/detect')
        .then(response => response.text())
        .then(text => {
          status.textContent = '';
          result.textContent = text;
        })
        .catch(error => {
          status.textContent = 'Fehler: ' + error;
          result.textContent = '';
        });
    }
  </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!jpeg_converted) {
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) {
            break;
        }
    }

    return res;
}

static esp_err_t save_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Save handler triggered");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Kamera");
        return ESP_FAIL;
    }

    // Convert RGB565 to RGB888
    uint8_t *rgb888 = (uint8_t*)malloc(fb->width * fb->height * 3);
    if (!rgb888) {
        esp_camera_fb_return(fb);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Speicher");
        return ESP_FAIL;
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, rgb888);
    int width = fb->width;
    int height = fb->height;
    esp_camera_fb_return(fb);

    if (!converted) {
        free(rgb888);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Konvertierung");
        return ESP_FAIL;
    }

    bool success = sdcard::save_jpeg_with_timestamp(rgb888, width, height, "/sdcard/BeeSense");
    free(rgb888);

    httpd_resp_set_type(req, "text/plain");
    if (success) {
        httpd_resp_sendstr(req, "Foto gespeichert");
    } else {
        httpd_resp_sendstr(req, "Fehler: SD-Karte");
    }
    
    return success ? ESP_OK : ESP_FAIL;
}

static esp_err_t detect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Detect handler triggered");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Kamera");
        return ESP_FAIL;
    }

    // Convert RGB565 to RGB888
    uint8_t *rgb888 = (uint8_t*)malloc(fb->width * fb->height * 3);
    if (!rgb888) {
        esp_camera_fb_return(fb);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Speicher");
        return ESP_FAIL;
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, rgb888);
    int width = fb->width;
    int height = fb->height;
    esp_camera_fb_return(fb);

    if (!converted) {
        free(rgb888);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Fehler: Konvertierung");
        return ESP_FAIL;
    }

    // Run detection (mock for now)
    int bee_count = run_detection_mock();
    
    // Save with detection result
    sdcard::save_jpeg_with_detections(rgb888, width, height, bee_count, "/sdcard/BeeSense");
    free(rgb888);

    char response[64];
    snprintf(response, sizeof(response), "🐝 %d Hummel(n) erkannt (Mock)", bee_count);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, response);
    
    return ESP_OK;
}

static void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
    httpd_uri_t save_uri = {.uri = "/save", .method = HTTP_GET, .handler = save_handler, .user_ctx = NULL};
    httpd_uri_t detect_uri = {.uri = "/detect", .method = HTTP_GET, .handler = detect_handler, .user_ctx = NULL};
    httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};

    ESP_LOGI(TAG, "Starting web server on port 80");
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &save_uri);
        httpd_register_uri_handler(camera_httpd, &detect_uri);
    }

    config.server_port = 81;
    config.ctrl_port = 32769;
    httpd_handle_t stream_httpd = NULL;
    
    ESP_LOGI(TAG, "Starting stream server on port 81");
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Bumblebee Detection Starting...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    init_wifi();
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait for connection

    // Initialize NTP
    init_ntp();
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    // Initialize SD card
    ESP_LOGI(TAG, "Mounting SD card...");
    if (!sdcard::init()) {
        ESP_LOGE(TAG, "SD card init failed");
        return;
    }
    sdcard::create_dir("/sdcard/BeeSense");

    // Initialize camera
    if (ESP_OK != init_camera()) {
        ESP_LOGE(TAG, "Camera initialization failed");
        return;
    }
    
    // Start webserver
    start_webserver();

    ESP_LOGI(TAG, "System ready!");

    while (true) {
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
