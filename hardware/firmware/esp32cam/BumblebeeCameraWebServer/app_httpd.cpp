// Simplified HTTP server for ESP32-S3 Camera
// Features: QVGA stream + capture button only

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"
#include "SD.h"
#include <time.h>

httpd_handle_t camera_httpd = NULL;

// Helper function for timestamp
static String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    char buf[32];
    snprintf(buf, sizeof(buf), "no_time_%lu", (unsigned long)(millis() / 1000));
    return String(buf);
  }
  
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  
  char finalName[40];
  snprintf(finalName, sizeof(finalName), "%s-%03lu", buf, (unsigned long)(millis() % 1000));
  return String(finalName);
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Simple HTML page with stream and capture button
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-S3 Bumblebee Camera</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #1a1a1a;
      color: #fff;
      margin: 0;
      padding: 20px;
    }
    h1 {
      color: #ffd700;
      margin-bottom: 30px;
    }
    #stream {
      width: 100%;
      max-width: 640px;
      border: 3px solid #ffd700;
      border-radius: 10px;
      margin: 20px auto;
      display: block;
    }
    button {
      background-color: #ffd700;
      color: #1a1a1a;
      border: none;
      padding: 15px 30px;
      font-size: 18px;
      font-weight: bold;
      border-radius: 5px;
      cursor: pointer;
      margin: 10px;
      transition: background-color 0.3s;
    }
    button:hover {
      background-color: #ffed4e;
    }
    button:active {
      background-color: #ccaa00;
    }
    .info {
      margin-top: 20px;
      color: #888;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <h1>🐝 Bumblebee Camera</h1>
  <img id="stream" src="">
  <div>
    <button onclick="capturePhoto()">📷 Foto aufnehmen</button>
  </div>
  <div class="info">
    <p>Auflösung: QVGA (320x240)</p>
    <p id="status"></p>
  </div>
  
  <script>
    // Set stream URL with port 81
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
          status.textContent = 'Fehler beim Speichern: ' + error;
          console.error('Error:', error);
        });
    }
  </script>
</body>
</html>
)rawliteral";

// Stream handler
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
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted) {
        Serial.println("JPEG compression failed");
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

// Save handler - saves photo to SD card
static esp_err_t save_handler(httpd_req_t *req) {
  Serial.println("Save handler triggered");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "Fehler: Kamera konnte kein Foto aufnehmen");
    return ESP_FAIL;
  }

  String filename = "/BeeSense/" + getTimestamp() + ".jpg";
  Serial.printf("Saving to: %s\n", filename.c_str());

  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file on SD");
    esp_camera_fb_return(fb);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "Fehler: SD-Karte nicht bereit");
    return ESP_FAIL;
  }

  size_t written = file.write(fb->buf, fb->len);
  file.close();
  
  Serial.printf("Photo saved: %s (%u bytes written)\n", filename.c_str(), written);
  
  esp_camera_fb_return(fb);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  String response = "Foto gespeichert: " + filename;
  httpd_resp_sendstr(req, response.c_str());
  
  Serial.println("Response sent to client");
  return ESP_OK;
}

// Index handler
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

extern "C" void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };

  httpd_uri_t save_uri = {
    .uri = "/save",
    .method = HTTP_GET,
    .handler = save_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  // Start main server for web interface and save handler
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &save_uri);
  }

  // Start separate stream server on different port
  config.server_port = 81;
  config.ctrl_port = 32769;
  httpd_handle_t stream_httpd = NULL;
  
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}
