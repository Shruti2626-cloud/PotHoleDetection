#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>

// ================= SETTINGS =================
#define MPU_ADDR 0x69
#define SDA_PIN 21
#define SCL_PIN 22

const char* ssid = "ESP32_AirMonitor";
const char* password = "12345678";

// Threshold for pothole detection
#define POTHOLE_THRESHOLD 20000  

// ============================================

WebServer server(80);
RTC_DS3231 rtc;

int16_t ax, ay, az;
bool potholeDetected = false;
int potholeID = 1;

// ----------- MPU WAKE FUNCTION -------------
void initMPU() {
  Serial.println("Initializing MPU6500...");

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // Power management register
  Wire.write(0x00);   // Wake up
  Wire.endTransmission();

  delay(100);
  Serial.println("MPU Ready!");
}

// ----------- READ ACCEL --------------------
void readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();

  if (abs(ax) > POTHOLE_THRESHOLD ||
      abs(ay) > POTHOLE_THRESHOLD ||
      abs(az) > POTHOLE_THRESHOLD) {
    potholeDetected = true;
  }
}

// ----------- WEB RESPONSE ------------------
void handleQuery() {

  readMPU();

  DateTime now = rtc.now();

  String json = "{";
  json += "\"pothole_id\":" + String(potholeID) + ",";
  json += "\"timestamp\":\"" + String(now.timestamp()) + "\",";
  json += "\"latitude\":0.000000,";
  json += "\"longitude\":0.000000,";
  json += "\"ax\":" + String(ax) + ",";
  json += "\"ay\":" + String(ay) + ",";
  json += "\"az\":" + String(az) + ",";
  json += "\"mpu_ok\":1,";
  json += "\"gps_ok\":0,";
  json += "\"rtc_ok\":1";
  json += "}";

  if (potholeDetected) {
    potholeID++;
    potholeDetected = false;
  }

  server.send(200, "application/json", json);
}

// ================= SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);

  initMPU();

  // RTC
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("RTC NOT found!");
  } else {
    Serial.println("RTC found.");
  }

  // WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("\nAccess Point Started!");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/query", handleQuery);
  server.begin();

  Serial.println("Server started.");
  Serial.println("Open browser:");
  Serial.println("http://192.168.4.1/query?pothole_id=1");
}

// ================= LOOP ====================
void loop() {
  server.handleClient();
}