#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPS++.h>
#include <RTClib.h>
#include <WiFi.h>
#include "esp_http_server.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- WIFI CONFIGURATION ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- PINS ---
#define I2C_SDA 15
#define I2C_SCL 14
#define GPS_RX_PIN 12 
#define GPS_TX_PIN 13 
#define MPU_ADDR 0x69 

Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
RTC_DS3231 rtc;
HardwareSerial gpsSerial(1);

// Global Data Store
float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
float latitude = 0.0, longitude = 0.0;
char dateTimeStr[25] = "2000-01-01 00:00:00";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// --- HTTP HANDLERS ---

// 1. Video Stream Handler
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if(!jpeg_converted){
                Serial.println("JPEG compression failed");
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

// 2. Sensor Data Handler (JSON)
static esp_err_t sensors_handler(httpd_req_t *req) {
    char json_response[256];
    snprintf(json_response, sizeof(json_response), 
             "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"lat\":%.6f,\"lon\":%.6f,\"datetime\":\"%s\"}", 
             accelX, accelY, accelZ, latitude, longitude, dateTimeStr);
             
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler, // Root -> Stream
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t sensor_uri = {
    .uri       = "/sensors",
    .method    = HTTP_GET,
    .handler   = sensors_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &sensor_uri);
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    Serial.printf("Video Stream: http://%s/stream\n", WiFi.localIP().toString().c_str());
    Serial.printf("Sensor Data: http://%s/sensors\n", WiFi.localIP().toString().c_str());
  }
}


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // 1. Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // 2. Initialize MPU6050
  if (!mpu.begin(MPU_ADDR)) {
    Serial.println("MPU6050 not found! Checking fallback 0x68...");
    if(mpu.begin(0x68)) { Serial.println("Found MPU at 0x68!"); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  // 3. Initialize RTC
  if (!rtc.begin()) { Serial.println("RTC not found!"); }
  if (rtc.lostPower()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }

  // 4. Initialize GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // 5. Initialize Camera
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
  config.fb_count = 2; // Double buffering for smoother stream

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // 6. Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // 7. Start Server
  startCameraServer();
}

void loop() {
  // --- READ SENSORS ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  accelX = a.acceleration.x;
  accelY = a.acceleration.y;
  accelZ = a.acceleration.z; // Store for global access

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  DateTime now = rtc.now();
  sprintf(dateTimeStr, "%04d-%02d-%02d %02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());

  // Loop delay is minimal to allow HTTP server to respond quickly
  delay(10); 
}
