/*************************************************
 *  ESP32-CAM + MPU6050 + GPS + RTC
 *  Live MJPEG stream + JSON sensor API
 *  
 *  Fixed: I2C pin conflicts, type conflicts, error handling
 *************************************************/

#include "esp_camera.h"   // MUST be first
#include "camera_pins.h"   // Camera pins immediately after

#include <Wire.h>
#include <WiFi.h>
#include "esp_http_server.h"
#include <TinyGPS++.h>
#include <RTClib.h>

// Rename the conflicting sensor_t type from Adafruit library
#define sensor_t adafruit_sensor_t
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#undef sensor_t

/* ================= WIFI ================= */
const char* ssid = "Sanskar's S24 FE";
const char* password = "q7ptka3g42g5xay";

/* ================= PINS - FIXED FOR ESP32-CAM ================= */
#define I2C_SDA 2       // Changed from 15 (conflicts with camera)
#define I2C_SCL 4       // Changed from 14 (conflicts with camera)
#define GPS_RX_PIN 12
#define GPS_TX_PIN 13

/* ================= STREAM MACROS ================= */
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* ================= OBJECTS ================= */
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
RTC_DS3231 rtc;
HardwareSerial gpsSerial(1);

/* ================= GLOBAL DATA ================= */
float accelX = 0, accelY = 0, accelZ = 0;
float latitude = 0, longitude = 0;
char dateTimeStr[25] = "No RTC";

bool mpuOK = false;
bool rtcOK = false;

httpd_handle_t camera_httpd = NULL;

/* ================= STREAM HANDLER ================= */
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;

  if (httpd_resp_set_type(req, _STREAM_CONTENT_TYPE) != ESP_OK) {
    return ESP_FAIL;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }

    char header[64];
    size_t hlen = snprintf(header, sizeof(header), _STREAM_PART, fb->len);

    if (httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK ||
        httpd_resp_send_chunk(req, header, hlen) != ESP_OK ||
        httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK) {
      esp_camera_fb_return(fb);
      return ESP_FAIL;
    }

    esp_camera_fb_return(fb);
  }
}

/* ================= SENSOR JSON HANDLER ================= */
static esp_err_t sensors_handler(httpd_req_t *req) {
  char json[300];

  snprintf(json, sizeof(json),
           "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
           "\"lat\":%.6f,\"lon\":%.6f,\"time\":\"%s\","
           "\"mpu_ok\":%s,\"rtc_ok\":%s}",
           accelX, accelY, accelZ,
           latitude, longitude, dateTimeStr,
           mpuOK ? "true" : "false",
           rtcOK ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

/* ================= START SERVER ================= */
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler
  };

  httpd_uri_t sensor_uri = {
    .uri = "/sensors",
    .method = HTTP_GET,
    .handler = sensors_handler
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &sensor_uri);
    Serial.println("HTTP server started successfully");
  } else {
    Serial.println("HTTP server failed to start");
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32-CAM Sensor System Starting...");
  Serial.println("========================================\n");

  /* I2C - FIXED PINS */
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("I2C initialized on GPIO2 (SDA) & GPIO4 (SCL)");

  /* MPU6050 */
  Serial.print("Scanning for MPU6050... ");
  if (mpu.begin(0x68, &Wire)) {
    Serial.println("Found at 0x68 (OK)");
    mpuOK = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else if (mpu.begin(0x69, &Wire)) {
    Serial.println("Found at 0x69 (OK)");
    mpuOK = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("NOT FOUND (ERROR)");
    Serial.println("  Check: Wiring, pull-up resistors, power");
    mpuOK = false;
  }

  /* RTC */
  Serial.print("Initializing RTC DS3231... ");
  if (rtc.begin()) {
    Serial.println("OK");
    rtcOK = true;
    if (rtc.lostPower()) {
      Serial.println("  RTC lost power - setting compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    Serial.println("NOT FOUND (ERROR)");
    Serial.println("  Check: Wiring, I2C address (0x68)");
    rtcOK = false;
  }

  /* GPS */
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS initialized on GPIO12/13");

  /* CAMERA CONFIG */
  Serial.print("Initializing camera... ");
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if (esp_camera_init(&config) == ESP_OK) {
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
    Serial.println("Camera init failed - system halted");
    while (true) {
      delay(1000);
    }
  }

  /* WIFI */
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED");
    Serial.println("WiFi connection failed - check SSID/password");
  }

  startCameraServer();

  Serial.println("\n========================================");
  Serial.println("Setup Complete!");
  Serial.println("========================================");
  Serial.println("Endpoints:");
  Serial.print("  Video Stream: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");
  Serial.print("  Sensor Data:  http://");
  Serial.print(WiFi.localIP());
  Serial.println("/sensors");
  Serial.println("========================================\n");
}

/* ================= LOOP ================= */
void loop() {
  // Read MPU6050 if available
  if (mpuOK) {
    sensors_event_t a, g, t;
    if (mpu.getEvent(&a, &g, &t)) {
      accelX = a.acceleration.x;
      accelY = a.acceleration.y;
      accelZ = a.acceleration.z;
    }
  }

  // Read GPS
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  // Read RTC if available
  if (rtcOK) {
    DateTime now = rtc.now();
    sprintf(dateTimeStr, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
  }

  delay(10);
}
