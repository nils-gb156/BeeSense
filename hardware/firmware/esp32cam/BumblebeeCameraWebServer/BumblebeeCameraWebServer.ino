#include "esp_camera.h"
#include <WiFi.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "driver/rtc_io.h"
#include <EEPROM.h>
#include "esp_http_server.h"
#include "camera_pins.h"

#define EEPROM_SIZE 1

// SD-Pins (senseBox_Eye Stil)
#define VSPI_MISO   40 // DAT
#define VSPI_MOSI   38 // CMD
#define VSPI_SCLK   39 // CLK
#define VSPI_SS     41 // CS
#define SD_ENABLE   48
SPIClass sdspi = SPIClass();

int pictureNumber = 0;

const char *ssid = "FRITZ!Box 7560 LC GB";
const char *password = "48991416100471872244";

void startCameraServer();

void setup() {
  Serial.begin(115200);

  // WLAN zuerst verbinden
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Zeitzone Europe/Berlin und NTP konfigurieren
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  // Auf Zeit warten (max ~5s), damit getLocalTime zuverlässig ist
  {
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
  }

  // Kamera konfigurieren
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA; // 640 x 480
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  // SD initialisieren (SPI)
  pinMode(SD_ENABLE, OUTPUT);
  digitalWrite(SD_ENABLE, LOW);
  sdspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);
  if(!SD.begin(VSPI_SS, sdspi)){
    Serial.println("Card Mount Failed");
    return;
  }
  if(SD.cardType() == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("SD card initialized");

  // EEPROM initialisieren
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0);

  // Webserver starten
  startCameraServer();

  Serial.print("Camera Ready! Open http://");
  Serial.print(WiFi.localIP());
  Serial.println(" in your browser");
}

void loop() {
  delay(10000);
}
