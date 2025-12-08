#include "esp_http_server.h"
#include "esp_camera.h"
#include "SD.h"
#include <EEPROM.h>
#include <time.h>

// Hilfsfunktion für Zeitstempel
static String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        // Fallback: Sekunden seit Boot + Millisekunden anhängen
        char fb[32];
        snprintf(fb, sizeof(fb), "no_time_%lu-%03lu", (unsigned long)(millis()/1000), (unsigned long)(millis()%1000));
        return String(fb);
    }
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    // Millisekunden für eindeutige Namen bei >= 1 FPS
    char finalName[40];
    snprintf(finalName, sizeof(finalName), "%s-%03lu", buf, (unsigned long)(millis()%1000));
    return String(finalName);
}

extern int pictureNumber; // aus .ino

// HTML Oberfläche: Snapshot wird per JS regelmäßig neu geladen
static const char* INDEX_HTML =
"<!DOCTYPE html><html><head><title>Bumblebee Camera</title></head><body>"
"<h2>Bumblebee Camera Webserver</h2>"
"<img id=\"view\" src=\"/snapshot\" width=\"640\" height=\"480\" />"
"<br><button onclick=\"savePhoto()\">Einzelbild speichern</button>"
"<br><button onclick=\"burstPhotos()\">Serie 5s @ 3 FPS</button>"
"<p id='status'></p>"
"<script>"
"function refresh(){"
"  const img = document.getElementById('view');"
"  img.src = '/snapshot?ts=' + Date.now();"
"}"
"setInterval(refresh, 333);"   // 3 FPS Vorschau
"function savePhoto(){"
"  fetch('/save')"
"    .then(r => r.text())"
"    .then(t => { document.getElementById('status').innerText = t; })"
"}"
"function burstPhotos(){"
"  fetch('/burst')"
"    .then(r => r.text())"
"    .then(t => { document.getElementById('status').innerText = t; })"
"}"
"</script>"
"</body></html>";


// Startseite
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

// Snapshot-Handler: genau EIN Bild pro Request
static esp_err_t snapshot_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// Foto speichern Handler
static esp_err_t save_handler(httpd_req_t *req) {
    Serial.println("Save handler triggered");

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera capture failed");
        return ESP_FAIL;
    }

    // Ordner BeeSense anlegen, falls nicht vorhanden
    if (!SD.exists("/BeeSense")) {
        SD.mkdir("/BeeSense");
    }

    String filename = "/BeeSense/" + getTimestamp() + ".jpg";

    Serial.printf("Opening file: %s\n", filename.c_str());
    File file = SD.open(filename.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file");
        esp_camera_fb_return(fb);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }

    file.write(fb->buf, fb->len);
    file.close();
    esp_camera_fb_return(fb);

    Serial.printf("Saved photo to %s\n", filename.c_str());
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, ("Foto gespeichert: " + filename).c_str());
    return ESP_OK;
}

static esp_err_t burst_handler(httpd_req_t *req) {
    Serial.println("Burst handler triggered");

    // Ordner BeeSense anlegen
    if (!SD.exists("/BeeSense")) {
        SD.mkdir("/BeeSense");
    }

    // 5 Sekunden lang alle 333 ms ein Foto
    unsigned long start = millis();
    int count = 0;
    while (millis() - start < 5000) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
            continue;
        }

        String filename = "/BeeSense/" + getTimestamp() + ".jpg";
        File file = SD.open(filename.c_str(), FILE_WRITE);
        if (file) {
            file.write(fb->buf, fb->len);
            file.close();
            Serial.printf("Saved burst photo: %s\n", filename.c_str());
            count++;
        }
        esp_camera_fb_return(fb);

        delay(333); // 3 FPS
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, ("Serie beendet, " + String(count) + " Fotos gespeichert").c_str());
    return ESP_OK;
}

// Server starten
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri    = { .uri="/",        .method=HTTP_GET, .handler=index_handler    };
        httpd_uri_t snapshot_uri = { .uri="/snapshot",.method=HTTP_GET, .handler=snapshot_handler };
        httpd_uri_t save_uri     = { .uri="/save",    .method=HTTP_GET, .handler=save_handler     };
        httpd_uri_t burst_uri = { .uri="/burst", .method=HTTP_GET, .handler=burst_handler };
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &snapshot_uri);
        httpd_register_uri_handler(server, &save_uri);
        httpd_register_uri_handler(server, &burst_uri);
    }
}
