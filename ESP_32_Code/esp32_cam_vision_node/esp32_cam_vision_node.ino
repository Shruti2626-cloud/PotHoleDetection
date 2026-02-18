/*************************************************
 *  ESP32-CAM VISION NODE (Dual ESP32 Architecture)
 *  
 *  See docs/HARDWARE.md and docs/DETAIL.md for wiring
 *  and system architecture details.
 *************************************************/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_task_wdt.h"

/* ================= CAMERA PIN DEFINITIONS ================= */
// AI Thinker ESP32-CAM pin configuration
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* ================= WIFI CONFIGURATION ================= */
const char* ssid = "Sanskar's S24 FE";
const char* password = "q7ptka3g42g5xay";

/* ================= STREAM CONFIGURATION ================= */
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* ================= GLOBAL OBJECTS ================= */
httpd_handle_t stream_httpd = NULL;

/* ================= MJPEG STREAM HANDLER ================= */
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  
  // Set response type to MJPEG stream
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  
  // Enable CORS for web-based frontends
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  // Continuous streaming loop
  while (true) {
    // Capture frame from camera
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }
    
    // Create separate buffer for HTTP header (CRITICAL: Don't corrupt frame buffer!)
    char part_buf[64];
    size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
    
    // Send boundary
    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }
    
    // Send header
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }
    
    // Send JPEG data
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }
    
    // Return frame buffer to driver
    esp_camera_fb_return(fb);
    fb = NULL;
  }
  
  return res;
}

/* ================= HEALTH CHECK HANDLER ================= */
static esp_err_t health_handler(httpd_req_t *req) {
  const char* response = "{\"status\":\"ok\",\"node\":\"vision\",\"uptime\":%lu}";
  char json[100];
  snprintf(json, sizeof(json), response, millis() / 1000);
  
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

/* ================= START HTTP SERVER ================= */
void startStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;  // Port 81 for stream
  config.ctrl_port = 32768;
  
  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };
  
  httpd_uri_t health_uri = {
    .uri = "/health",
    .method = HTTP_GET,
    .handler = health_handler,
    .user_ctx = NULL
  };
  
  Serial.print("Starting stream server on port 81... ");
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &health_uri);
    Serial.println("SUCCESS");
  } else {
    Serial.println("FAILED");
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32-CAM VISION NODE");
  Serial.println("Dual ESP32 Architecture v2.0");
  Serial.println("========================================\n");
  
  /* WATCHDOG TIMER INITIALIZATION */
  Serial.print("Initializing watchdog timer (30s)... ");
  esp_task_wdt_init(30, true);  // 30 second timeout
  esp_task_wdt_add(NULL);       // Add current task
  Serial.println("OK");
  
  /* CAMERA INITIALIZATION */
  Serial.print("Initializing OV2640 camera... ");
  
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
  
  // Frame size: QVGA (320x240) for balance of quality and speed
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;  // 10-63, lower = better quality
  config.fb_count = 2;       // Double buffering
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("FAILED (Error: 0x%x)\n", err);
    Serial.println("Camera init failed - system halted");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("SUCCESS");
  
  // Camera sensor settings
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 = No Effect
    s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable, 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable
    s->set_aec2(s, 0);           // 0 = disable, 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable, 1 = enable
    s->set_wpc(s, 1);            // 0 = disable, 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable, 1 = enable
    s->set_lenc(s, 1);           // 0 = disable, 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
    s->set_vflip(s, 0);          // 0 = disable, 1 = enable
    s->set_dcw(s, 1);            // 0 = disable, 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable, 1 = enable
    Serial.println("Camera settings configured");
  }
  
  /* WIFI CONNECTION */
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" CONNECTED");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println(" FAILED");
    Serial.println("WiFi connection failed - check SSID/password");
    Serial.println("System will continue but streaming won't work");
  }
  
  /* START STREAM SERVER */
  startStreamServer();
  
  Serial.println("\n========================================");
  Serial.println("VISION NODE READY");
  Serial.println("========================================");
  Serial.println("Endpoints:");
  Serial.print("  MJPEG Stream: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");
  Serial.print("  Health Check: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/health");
  Serial.println("========================================\n");
  Serial.println("Vision node is now streaming...");
}

/* ================= LOOP ================= */
void loop() {
  // Reset watchdog timer to prevent false timeouts
  esp_task_wdt_reset();
  
  // Vision node is purely event-driven via HTTP requests
  // No continuous processing needed in loop
  
  // Monitor WiFi connection and system health
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {  // Check every 10 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }
    
    // Memory monitoring (detect leaks)
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    lastCheck = millis();
  }
  
  delay(100);
}
