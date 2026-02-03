# ESP32-CAM Pothole Detection Wiring Guide

This guide details how to connect the MPU6050, GPS (Neo-6M/Small), and DS3231 RTC to the ESP32-CAM (AI-Thinker).

> [!WARNING]
> **I2C Address Conflict Fix**: You MUST connect the **AD0** pin on the MPU6050 to **3.3V**. This sets its I2C address to `0x69` to avoid conflict with the RTC (`0x68`).

## Pinout Table

| Component | Pin | ESP32-CAM Pin | Notes |
| :--- | :--- | :--- | :--- |
| **Common** | VCC | 5V / 3.3V | Check module voltage requirements (usually 3.3V or 5V) |
| | GND | GND | Common Ground |
| **MPU6050** | SDA | GPIO 15 | I2C Data |
| | SCL | GPIO 14 | I2C Clock |
| | **AD0** | **3.3V** | **CRITICAL for Address 0x69** |
| **DS3231 RTC**| SDA | GPIO 15 | Shared I2C Bus |
| | SCL | GPIO 14 | Shared I2C Bus |
| **GPS** | TX | GPIO 12 | Connected to ESP32 RX |
| | RX | GPIO 13 | Connected to ESP32 TX |

## Connection Diagram Description

1.  **I2C Bus (Sensors)**:
    *   Connect MPU6050 **SDA** and DS3231 **SDA** to ESP32-CAM **GPIO 15**.
    *   Connect MPU6050 **SCL** and DS3231 **SCL** to ESP32-CAM **GPIO 14**.
    *   **Power**: Connect VCC/GND appropriately.
    *   **Address Selection**: Connect MPU6050 **AD0** pin directly to **3.3V**.

2.  **GPS (UART)**:
    *   Connect GPS **TX** pin to ESP32-CAM **GPIO 12**.
    *   Connect GPS **RX** pin to ESP32-CAM **GPIO 13**.
    *   *Note*: The code uses `Serial1` on pins 12 (RX) and 13 (TX). GPS TX sends data to ESP32 RX.

> [!CAUTION]
> **Boot Issue**: GPIO 12 is a strapping pin. If the GPS pulls it High during boot, the ESP32 may fail to flash or boot.
> **Workaround**: If you encounter boot issues, verify the GPS is not pulling this pin high on reset, or temporarily disconnect the GPS TX wire while powering on.
