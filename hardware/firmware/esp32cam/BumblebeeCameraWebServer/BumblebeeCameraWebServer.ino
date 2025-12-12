#include "esp_camera.h"
#include <WiFi.h>
#include "camera_pins.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// ===========================
// WiFi credentials
// ===========================
const char *ssid     = "FRITZ!Box 7560 LC GB";
const char *password = "48991416100471872244";

// ===========================
// SD Card Pins (ESP32-S3)
// ===========================
#define VSPI_MISO 40
#define VSPI_MOSI 38
#define VSPI_SCLK 39
#define VSPI_SS   41
#define SD_ENABLE 48
SPIClass sdspi = SPIClass();

extern "C" void startCameraServer();

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.setDebugOutput(true);
  Serial.println();

  // Camera config for ESP32-S3 with QVGA
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  // WiFi connect
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // NTP Time Sync (Europe/Berlin)
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  
  struct tm timeinfo;
  int retries = 50;
  while (retries-- > 0) {
    if (getLocalTime(&timeinfo)) {
      Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
        1900 + timeinfo.tm_year, 1 + timeinfo.tm_mon, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      break;
    }
    delay(100);
  }

  // SD Card init
  pinMode(SD_ENABLE, OUTPUT);
  digitalWrite(SD_ENABLE, LOW);
  sdspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);
  
  if (!SD.begin(VSPI_SS, sdspi)) {
    Serial.println("SD Card Mount Failed");
  } else if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
  } else {
    Serial.println("SD card initialized");
    if (!SD.exists("/BeeSense")) {
      SD.mkdir("/BeeSense");
      Serial.println("Created /BeeSense directory");
    }
  }

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // Do nothing. Everything is done in another task by the web server
  delay(10000);
}
