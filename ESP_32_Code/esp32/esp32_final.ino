#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>
#include "credentials.h"
#include "time.h"   // 🔥 for NTP

// I2C addresses
#define MPU_ADDR 0x69 // AD0 → VCC
#define SDA_PIN 21
#define SCL_PIN 22

WebServer server(80);
RTC_DS3231 rtc;

float ax_ms2 = 0, ay_ms2 = 0, az_ms2 = 0;
bool mpu_ok = false;
bool rtc_ok = false;

// -------- MPU Initialization --------
void initMPU() {
  Serial.println("Initializing MPU6050...");
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  byte error = Wire.endTransmission(true);

  if(error == 0){
    Serial.println("MPU6050 Ready");
    mpu_ok = true;
  } else {
    Serial.println("MPU6050 NOT detected!");
    mpu_ok = false;
  }
}

// -------- MPU Read --------
void readMPU() {
  if(!mpu_ok) return;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, 6, true);

  if(Wire.available() == 6){
    int16_t ax = Wire.read() << 8 | Wire.read();
    int16_t ay = Wire.read() << 8 | Wire.read();
    int16_t az = Wire.read() << 8 | Wire.read();

    ax_ms2 = (ax / 16384.0) * 9.81;
    ay_ms2 = (ay / 16384.0) * 9.81;
    az_ms2 = (az / 16384.0) * 9.81;

    Serial.print("AX: "); Serial.print(ax_ms2);
    Serial.print(" AY: "); Serial.print(ay_ms2);
    Serial.print(" AZ: "); Serial.println(az_ms2);
  }
}

// -------- HTTP Handler --------
void handleQuery() {
  readMPU();

  int req_id = 0;
  if(server.hasArg("pothole_id"))
    req_id = server.arg("pothole_id").toInt();

  String timestamp = "2000-01-01T00:00:00";
  if(rtc_ok){
    DateTime now = rtc.now();
    timestamp = now.timestamp(DateTime::TIMESTAMP_FULL);
  }

  String json = "{";
  json += "\"pothole_id\":" + String(req_id) + ",";
  json += "\"timestamp\":\"" + timestamp + "\",";
  json += "\"latitude\":0.0,";
  json += "\"longitude\":0.0,";
  json += "\"ax\":" + String(ax_ms2,2) + ",";
  json += "\"ay\":" + String(ay_ms2,2) + ",";
  json += "\"az\":" + String(az_ms2,2) + ",";
  json += "\"mpu_ok\":" + String(mpu_ok ? "true" : "false") + ",";
  json += "\"gps_ok\":false,";
  json += "\"rtc_ok\":" + String(rtc_ok ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// -------- Setup --------
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // MPU
  initMPU();

  // -------- WiFi FIRST --------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting WiFi...");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // -------- RTC --------
  Serial.println("Initializing RTC...");
  if(!rtc.begin()){
    Serial.println("RTC NOT found");
    rtc_ok = false;
  } else {
    Serial.println("RTC found");
    rtc_ok = true;

    // 🔥 Get correct IST time from internet
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // 19800 = +5:30

    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      Serial.println("Time synced from NTP");

      rtc.adjust(DateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      ));
    } else {
      Serial.println("Failed to get NTP time");
    }
  }

  // HTTP Server
  server.on("/query", handleQuery);
  server.begin();
  Serial.println("HTTP Server started");
}

// -------- Loop --------
void loop() {
  server.handleClient();
}
