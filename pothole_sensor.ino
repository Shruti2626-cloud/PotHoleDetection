#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPS++.h>
#include <RTClib.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- CONFIGURATION ---
// I2C Pins for MPU6050 and RTC
#define I2C_SDA 15
#define I2C_SCL 14

// GPS Serial Pins
#define GPS_RX_PIN 12 // Connect to GPS TX
#define GPS_TX_PIN 13 // Connect to GPS RX

// MPU6050 I2C Address
// 0x68 is default, but conflicts with DS3231 (also 0x68). 
// Connect AD0 pin on MPU6050 to 3.3V to select 0x69.
#define MPU_ADDR 0x69 

Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
RTC_DS3231 rtc;
HardwareSerial gpsSerial(1);

float accelZ, vibrationSeverity;
float latitude = 0.0, longitude = 0.0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- ESP32 Pothole Detection Start ---");

  // 1. Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("I2C initialized on SDA:15, SCL:14");

  // 2. Initialize MPU6050
  // Essential: Try to find MPU at address 0x69 first (Assuming AD0 is pulled HIGH)
  if (!mpu.begin(MPU_ADDR)) {
    Serial.println("Failed to find MPU6050 chip at 0x69!");
    Serial.println("Check wiring: AD0 must be connected to 3.3V.");
    // Fallback check just in case user didn't wire it yet, to confirm connection at least
    if(mpu.begin(0x68)) {
        Serial.println("WARNING: Found MPU6050 at 0x68. THIS CONFLICTS WITH RTC!");
    } else {
        Serial.println("MPU6050 not detected at all.");
        while (1) { delay(10); }
    }
  }
  
  // Configure MPU settings
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 Found!");

  // 3. Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC! Check permissions/wiring.");
    while (1);
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("RTC Found!");

  // 4. Initialize GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS Serial Initialized.");

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
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    while (1);
  }

  Serial.println("ESP32-CAM Ready.");
}

void loop() {
  // Read Sensors
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  accelZ = abs(a.acceleration.z);
  vibrationSeverity = accelZ / 10.0;

  // Read GPS
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  // Read Time
  DateTime now = rtc.now();

  // Capture Image
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // --- OUPUT DATA ---
  Serial.println("=== POTHOLE DATA ===");
  
  char timeBuffer[20];
  sprintf(timeBuffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          now.day(), now.month(), now.year(), 
          now.hour(), now.minute(), now.second());
  Serial.print("Timestamp: "); Serial.println(timeBuffer);

  Serial.print("Location: "); Serial.print(latitude, 6); 
  Serial.print(", "); Serial.println(longitude, 6);

  Serial.print("Z Accel: "); Serial.println(accelZ);
  Serial.print("Vib Severity: "); Serial.println(vibrationSeverity);
  Serial.print("Img Size: "); Serial.println(fb->len);

  Serial.println("====================\n");

  esp_camera_fb_return(fb);

  delay(3000); // 3 second interval
}
