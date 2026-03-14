#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>
#include "credentials.h"

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
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0);     // Wake up
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
  Wire.write(0x3B); // ACCEL_XOUT_H
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

  // RTC
  Serial.println("Initializing RTC...");
  if(!rtc.begin()){
    Serial.println("RTC NOT found");
    rtc_ok = false;
  } else {
    Serial.println("RTC found");
    rtc_ok = true;
    if(rtc.lostPower()){
      Serial.println("RTC lost power, setting time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting WiFi...");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // HTTP Server
  server.on("/query", handleQuery);
  server.begin();
  Serial.println("HTTP Server started");
}

// -------- Loop --------
void loop() {
  server.handleClient();
}