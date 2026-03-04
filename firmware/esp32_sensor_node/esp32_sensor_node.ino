/*************************************************
 *  ESP32 SENSOR NODE (Dual ESP32 Architecture)
 *  
 *  RESPONSIBILITIES:
 *  - Event-driven sensor reading (only when queried)
 *  - MPU6050 accelerometer data
 *  - NEO-6M GPS coordinates
 *  - DS3231 RTC timestamps
 *  - Return structured JSON response
 *  
 *  DOES NOT:
 *  - Stream continuously
 *  - Process video
 *  - Make decisions
 *  - Store logs
 *  
 *  ENDPOINTS:
 *  - http://<IP>/query?pothole_id=<ID>  → JSON sensor data
 *  - http://<IP>/health                 → Health check
 *  
 *  WIRING:
 *  MPU6050:
 *    - SDA → GPIO 21
 *    - SCL → GPIO 22
 *    - VCC → 3.3V
 *    - GND → GND
 *  
 *  NEO-6M GPS:
 *    - TX → GPIO 16 (RX2)
 *    - RX → GPIO 17 (TX2)
 *    - VCC → 3.3V
 *    - GND → GND
 *  
 *  DS3231 RTC:
 *    - SDA → GPIO 21 (shared I2C)
 *    - SCL → GPIO 22 (shared I2C)
 *    - VCC → 3.3V
 *    - GND → GND
 *  
 *  Author: Pothole Detection System
 *  Version: 2.0 (Dual ESP32 Architecture)
 *************************************************/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPS++.h>
#include <RTClib.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/* ================= WIFI CONFIGURATION ================= */
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

/* ================= PIN CONFIGURATION ================= */
#define I2C_SDA 21
#define I2C_SCL 22
#define GPS_RX_PIN 16  // ESP32 RX2 ← GPS TX
#define GPS_TX_PIN 17  // ESP32 TX2 → GPS RX

/* ================= GLOBAL OBJECTS ================= */
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
RTC_DS3231 rtc;
HardwareSerial gpsSerial(2);  // Use UART2
WebServer server(80);

/* ================= SENSOR STATUS FLAGS ================= */
bool mpuOK = false;
bool gpsOK = false;
bool rtcOK = false;

/* ================= SENSOR DATA CACHE ================= */
struct SensorData {
  float ax, ay, az;
  float latitude, longitude;
  String timestamp;
  bool mpu_ok, gps_ok, rtc_ok;
};

SensorData latestData;

/* ================= QUERY HANDLER (EVENT-DRIVEN) ================= */
void handleQuery() {
  // Check if pothole_id parameter exists
  if (!server.hasArg("pothole_id")) {
    server.send(400, "application/json", "{\"error\":\"Missing pothole_id parameter\"}");
    return;
  }
  
  int pothole_id = server.arg("pothole_id").toInt();
  
  Serial.println("\n========================================");
  Serial.printf("SENSOR QUERY RECEIVED: Pothole ID %d\n", pothole_id);
  Serial.println("========================================");
  
  unsigned long startTime = millis();
  
  // STEP 1: Read MPU6050 Accelerometer
  Serial.print("Reading MPU6050... ");
  if (mpuOK) {
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      latestData.ax = a.acceleration.x;
      latestData.ay = a.acceleration.y;
      latestData.az = a.acceleration.z;
      latestData.mpu_ok = true;
      Serial.printf("OK (%.2f, %.2f, %.2f)\n", latestData.ax, latestData.ay, latestData.az);
    } else {
      latestData.mpu_ok = false;
      Serial.println("READ FAILED");
    }
  } else {
    latestData.ax = 0.0;
    latestData.ay = 0.0;
    latestData.az = 0.0;
    latestData.mpu_ok = false;
    Serial.println("NOT INITIALIZED");
  }
  
  // STEP 2: Read GPS Coordinates
  Serial.print("Reading GPS... ");
  // Process GPS data for up to 100ms to get fresh fix
  unsigned long gpsStart = millis();
  while (millis() - gpsStart < 100) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
  }
  
  if (gps.location.isValid() && gps.location.age() < 2000) {
    latestData.latitude = gps.location.lat();
    latestData.longitude = gps.location.lng();
    latestData.gps_ok = true;
    Serial.printf("OK (%.6f, %.6f)\n", latestData.latitude, latestData.longitude);
  } else {
    latestData.latitude = 0.0;
    latestData.longitude = 0.0;
    latestData.gps_ok = false;
    Serial.println("NO FIX");
  }
  
  // STEP 3: Read RTC Timestamp
  Serial.print("Reading RTC... ");
  if (rtcOK) {
    DateTime now = rtc.now();
    char buffer[25];
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    latestData.timestamp = String(buffer);
    latestData.rtc_ok = true;
    Serial.printf("OK (%s)\n", buffer);
  } else {
    latestData.timestamp = "1970-01-01T00:00:00";
    latestData.rtc_ok = false;
    Serial.println("NOT INITIALIZED");
  }
  
  // STEP 4: Build JSON Response
  String json = "{";
  json += "\"pothole_id\":" + String(pothole_id) + ",";
  json += "\"timestamp\":\"" + latestData.timestamp + "\",";
  json += "\"latitude\":" + String(latestData.latitude, 6) + ",";
  json += "\"longitude\":" + String(latestData.longitude, 6) + ",";
  json += "\"ax\":" + String(latestData.ax, 2) + ",";
  json += "\"ay\":" + String(latestData.ay, 2) + ",";
  json += "\"az\":" + String(latestData.az, 2) + ",";
  json += "\"mpu_ok\":" + String(latestData.mpu_ok ? "true" : "false") + ",";
  json += "\"gps_ok\":" + String(latestData.gps_ok ? "true" : "false") + ",";
  json += "\"rtc_ok\":" + String(latestData.rtc_ok ? "true" : "false");
  json += "}";
  
  unsigned long responseTime = millis() - startTime;
  Serial.printf("Response time: %lu ms\n", responseTime);
  Serial.println("========================================\n");
  
  // STEP 5: Send JSON Response
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

/* ================= HEALTH CHECK HANDLER ================= */
void handleHealth() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"node\":\"sensor\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"mpu_ok\":" + String(mpuOK ? "true" : "false") + ",";
  json += "\"gps_ok\":" + String(gpsOK ? "true" : "false") + ",";
  json += "\"rtc_ok\":" + String(rtcOK ? "true" : "false");
  json += "}";
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

/* ================= 404 HANDLER ================= */
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32 SENSOR NODE");
  Serial.println("Dual ESP32 Architecture v2.0");
  Serial.println("========================================\n");
  
  /* I2C INITIALIZATION */
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("I2C initialized on GPIO%d (SDA) & GPIO%d (SCL)\n", I2C_SDA, I2C_SCL);
  
  /* MPU6050 INITIALIZATION */
  Serial.print("Initializing MPU6050... ");
  
  // Try 0x69 first (AD0 connected to 3.3V - RECOMMENDED to avoid RTC conflict)
  if (mpu.begin(0x69, &Wire)) {
    Serial.println("FOUND at 0x69 ✓ (AD0=HIGH, no conflict with RTC)");
    mpuOK = true;
    
    // Configure MPU6050
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);  // ±8g for pothole detection
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    
    Serial.println("  Range: ±8g");
    Serial.println("  Filter: 21 Hz");
  } 
  // Fallback to 0x68 (AD0 floating or GND - WARNING: Conflicts with RTC!)
  else if (mpu.begin(0x68, &Wire)) {
    Serial.println("FOUND at 0x68 ⚠️ (AD0=LOW)");
    Serial.println("  WARNING: I2C address conflict with RTC DS3231!");
    Serial.println("  SOLUTION: Connect MPU6050 AD0 pin to 3.3V");
    mpuOK = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } 
  else {
    Serial.println("NOT FOUND ✗");
    Serial.println("  Check: Wiring, I2C address, power");
    Serial.println("  Tip: Connect AD0 to 3.3V for address 0x69");
    mpuOK = false;
  }
  
  /* GPS INITIALIZATION */
  Serial.print("Initializing NEO-6M GPS... ");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.printf("OK (GPIO%d/GPIO%d)\n", GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("  Waiting for GPS fix (may take 30-60s outdoors)");
  
  // Wait up to 5 seconds for initial GPS data
  unsigned long gpsWaitStart = millis();
  while (millis() - gpsWaitStart < 5000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isValid()) {
      gpsOK = true;
      Serial.printf("  GPS FIX ACQUIRED: %.6f, %.6f ✓\n", 
                    gps.location.lat(), gps.location.lng());
      break;
    }
  }
  
  if (!gpsOK) {
    Serial.println("  No GPS fix yet (will continue trying)");
  }
  
  /* RTC INITIALIZATION */
  Serial.print("Initializing DS3231 RTC... ");
  if (rtc.begin()) {
    Serial.println("FOUND ✓");
    rtcOK = true;
    
    if (rtc.lostPower()) {
      Serial.println("  RTC lost power - setting compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    DateTime now = rtc.now();
    Serial.printf("  Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
  } else {
    Serial.println("NOT FOUND ✗");
    Serial.println("  Check: Wiring, I2C address (0x68)");
    rtcOK = false;
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
    Serial.println(" CONNECTED ✓");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println(" FAILED ✗");
    Serial.println("WiFi connection failed - check SSID/password");
  }
  
  /* HTTP SERVER SETUP */
  server.on("/query", HTTP_GET, handleQuery);
  server.on("/health", HTTP_GET, handleHealth);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started on port 80");
  
  Serial.println("\n========================================");
  Serial.println("SENSOR NODE READY");
  Serial.println("========================================");
  Serial.println("Endpoints:");
  Serial.print("  Sensor Query: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/query?pothole_id=<ID>");
  Serial.print("  Health Check: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/health");
  Serial.println("========================================");
  Serial.printf("Sensors: MPU=%s GPS=%s RTC=%s\n",
                mpuOK ? "OK" : "FAIL",
                gpsOK ? "OK" : "WAIT",
                rtcOK ? "OK" : "FAIL");
  Serial.println("========================================\n");
  Serial.println("Sensor node is idle, waiting for queries...");
}

/* ================= LOOP ================= */
void loop() {
  // Handle HTTP requests (event-driven)
  server.handleClient();
  
  // Background GPS processing (keep GPS data fresh)
  while (gpsSerial.available()) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid() && !gpsOK) {
        gpsOK = true;
        Serial.printf("GPS FIX ACQUIRED: %.6f, %.6f\n", 
                      gps.location.lat(), gps.location.lng());
      }
    }
  }
  
  // Monitor WiFi connection
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {  // Check every 10 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }
    
    // Memory monitoring (detect leaks)
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    lastWiFiCheck = millis();
  }
  
  delay(10);  // Small delay to prevent watchdog issues
}
