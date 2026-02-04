# ESP32-CAM Wiring Diagram
## Complete System: Camera + MPU6050 + GPS + RTC DS3231

---

## ⚠️ CRITICAL NOTES

1. **I2C Address Conflict:** MPU6050 and DS3231 both use address `0x68` by default
   - **Solution:** Connect MPU6050's AD0 pin to 3.3V to change its address to `0x69`

2. **Pull-Up Resistors Required:** I2C bus needs 4.7kΩ pull-ups on SDA and SCL

3. **Power:** ESP32-CAM with camera needs **stable 5V power supply** (min 500mA)
   - USB power alone may not be enough
   - Use external 5V power adapter recommended

4. **Common Ground:** All components MUST share common ground

---

## 📋 Components List

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32-CAM (AI Thinker) | 1 | Main controller with camera |
| MPU6050 | 1 | 6-axis accelerometer/gyroscope |
| DS3231 RTC | 1 | Real-time clock module |
| GPS Module (NEO-6M or similar) | 1 | UART GPS with TX/RX |
| 4.7kΩ Resistors | 2 | I2C pull-ups (can use 2.2kΩ - 10kΩ) |
| Jumper Wires | Multiple | For connections |
| 5V Power Supply | 1 | Min 500mA, 1A recommended |
| Breadboard | 1 | Optional, for prototyping |

---

## 🔌 Pin Assignments

| Function | ESP32-CAM GPIO | Connected To |
|----------|----------------|--------------|
| I2C SDA | GPIO 2 | MPU6050 SDA + RTC SDA + Pull-up |
| I2C SCL | GPIO 4 | MPU6050 SCL + RTC SCL + Pull-up |
| GPS RX | GPIO 12 | GPS TX |
| GPS TX | GPIO 13 | GPS RX |
| 5V | 5V pin | All module VCC (or 3.3V for sensors) |
| 3.3V | 3.3V pin | MPU6050 VCC + Pull-up resistors |
| GND | GND | Common ground for all modules |

---

## 🔧 Component Wiring

### 1. ESP32-CAM Power
```
Power Supply (5V, 1A)
├─ Positive (+) ──────→ ESP32-CAM 5V pin
└─ Negative (-) ──────→ ESP32-CAM GND pin
```

### 2. MPU6050 (Accelerometer/Gyroscope)
```
MPU6050          ESP32-CAM
─────────────────────────────
VCC    ──────→   3.3V
GND    ──────→   GND
SCL    ──────→   GPIO 4 (I2C SCL)
SDA    ──────→   GPIO 2 (I2C SDA)
AD0    ──────→   3.3V  ⚠️ IMPORTANT: Sets address to 0x69
INT    ──────→   (Not connected)
```

**Notes:**
- MPU6050 operates at **3.3V**
- **AD0 to 3.3V is CRITICAL** to avoid address conflict with RTC
- If AD0 is left floating or connected to GND, address will be 0x68 (conflict!)

### 3. DS3231 RTC (Real-Time Clock)
```
DS3231           ESP32-CAM
─────────────────────────────
VCC    ──────→   5V (or 3.3V, module accepts both)
GND    ──────→   GND
SCL    ──────→   GPIO 4 (I2C SCL) - shared with MPU6050
SDA    ──────→   GPIO 2 (I2C SDA) - shared with MPU6050
32K    ──────→   (Not connected)
SQW    ──────→   (Not connected)
```

**Notes:**
- DS3231 has I2C address **0x68** (fixed, cannot change)
- Most DS3231 modules have **built-in pull-up resistors**
- Battery backup keeps time when power is off

### 4. GPS Module (NEO-6M or similar)
```
GPS Module       ESP32-CAM
─────────────────────────────
VCC    ──────→   5V (or 3.3V, check your module)
GND    ──────→   GND
TX     ──────→   GPIO 12 (ESP32 RX)
RX     ──────→   GPIO 13 (ESP32 TX)
PPS    ──────→   (Not connected)
```

**Notes:**
- GPS TX connects to ESP32 RX (data flows GPS → ESP32)
- GPS RX connects to ESP32 TX (data flows ESP32 → GPS)
- Some GPS modules need 5V, others work with 3.3V - check your module!
- GPS needs **clear view of sky** for satellite lock

### 5. I2C Pull-Up Resistors ⚠️ REQUIRED
```
              3.3V
                │
                ├──[ 4.7kΩ ]──→ GPIO 2 (SDA)
                │
                └──[ 4.7kΩ ]──→ GPIO 4 (SCL)
```

**Notes:**
- I2C **requires** pull-up resistors
- Use 4.7kΩ (or anywhere from 2.2kΩ to 10kΩ)
- Connect to **3.3V**, not 5V
- Some modules have built-in pull-ups, but adding external ones ensures reliable operation

---

## 📐 Complete Wiring Diagram (Text Format)

```
                                    ┌─────────────────┐
                                    │   5V POWER      │
                                    │   SUPPLY        │
                                    └────┬───────┬────┘
                                         │       │
                                        5V      GND
                                         │       │
        ┌────────────────────────────────┼───────┼────────────────────┐
        │                                │       │                    │
        │  ESP32-CAM                     │       │                    │
        │  ┌──────────────┐              │       │                    │
        │  │   CAMERA     │              │       │                    │
        │  └──────────────┘              │       │                    │
        │                                │       │                    │
        │  GPIO 2 (SDA) ●────────────┬───┼───────┼──────┬─────────┐   │
        │  GPIO 4 (SCL) ●────────────┼───┼───────┼──┬───┼─────┐   │   │
        │  GPIO 12 (RX) ●────────────┼───┼───────┼──┼───┼─────┼───┼───┼──→ GPS TX
        │  GPIO 13 (TX) ●────────────┼───┼───────┼──┼───┼─────┼───┼───┼──→ GPS RX
        │  3.3V ●────────────────────┼───┼───────┼──┼───┼─────┼───┼───┼──┐
        │  5V ●──────────────────────┼───┼───────●  │   │     │   │   │  │
        │  GND ●─────────────────────┼───┼──────────┼───┼─────┼───┼───┼──┼──→ Common GND
        └────────────────────────────┼───┼──────────┼───┼─────┼───┼───┼──┘
                                     │   │          │   │     │   │   │
                            4.7kΩ ┌──┘   │          │   │     │   │   │
                                 │       │          │   │     │   │   │
                            3.3V─┴───────┘          │   │     │   │   │
                                                    │   │     │   │   │
                            4.7kΩ ┌─────────────────┘   │     │   │   │
                                 │                      │     │   │   │
                            3.3V─┴──────────────────────┘     │   │   │
                                                              │   │   │
        ┌─────────────────────────────────────────────────────┘   │   │
        │  MPU6050                                                │   │
        │  ┌─────────┐                                            │   │
        │  │  SENSOR │                                            │   │
        │  └─────────┘                                            │   │
        │  VCC ──→ 3.3V (from ESP32)                              │   │
        │  GND ──→ Common GND                                     │   │
        │  SCL ──→ GPIO 4 (shared) ●──────────────────────────────┘   │
        │  SDA ──→ GPIO 2 (shared) ●──────────────────────────────────┘
        │  AD0 ──→ 3.3V  ⚠️ (Sets address to 0x69)                │
        └─────────────────────────────────────────────────────────────┘

        ┌─────────────────────────────────────────────────────────────┐
        │  DS3231 RTC                                                 │
        │  ┌──────────┐                                               │
        │  │   CLOCK  │                                               │
        │  │ [BATTERY]│                                               │
        │  └──────────┘                                               │
        │  VCC ──→ 5V (from ESP32)                                    │
        │  GND ──→ Common GND                                         │
        │  SCL ──→ GPIO 4 (shared with MPU6050)                       │
        │  SDA ──→ GPIO 2 (shared with MPU6050)                       │
        │  Address: 0x68 (fixed)                                      │
        └─────────────────────────────────────────────────────────────┘

        ┌─────────────────────────────────────────────────────────────┐
        │  GPS Module (NEO-6M)                                        │
        │  ┌──────────┐                                               │
        │  │  ANTENNA │                                               │
        │  └──────────┘                                               │
        │  VCC ──→ 5V (check your module - some need 3.3V)            │
        │  GND ──→ Common GND                                         │
        │  TX  ──→ GPIO 12 (ESP32 RX)                                 │
        │  RX  ──→ GPIO 13 (ESP32 TX)                                 │
        └─────────────────────────────────────────────────────────────┘
```

---

## 🧪 Testing Checklist

### Before Powering On:
- [ ] All GND connections are common
- [ ] Pull-up resistors installed (4.7kΩ on SDA and SCL to 3.3V)
- [ ] MPU6050 AD0 pin connected to 3.3V
- [ ] No short circuits between power and ground
- [ ] 5V power supply can provide at least 500mA

### After Powering On:
- [ ] Check Serial Monitor (115200 baud)
- [ ] WiFi connects and shows IP address
- [ ] MPU6050 detected at 0x69
- [ ] RTC detected at 0x68
- [ ] Camera initializes successfully
- [ ] GPS starts receiving data (may take 30-60 seconds for satellite lock)

### Access Points:
- [ ] Video stream: `http://[ESP32-IP]/stream`
- [ ] Sensor data: `http://[ESP32-IP]/sensors`

---

## 🔍 Troubleshooting

### I2C Devices Not Found
1. **Check pull-up resistors** - Most common issue!
2. **Verify wiring** - Especially SDA/SCL not swapped
3. **Check MPU6050 AD0 pin** - Must be connected to 3.3V
4. **Test with I2C scanner** (add scanner code to setup)
5. **Check power** - Ensure 3.3V is stable

### GPS Not Getting Fix
1. **Position GPS antenna** near window or outdoors
2. **Wait 30-60 seconds** for satellite lock (cold start)
3. **Check TX/RX not swapped** - GPS TX → ESP32 GPIO 12

### Camera Not Working
1. **Check power supply** - Camera needs significant current
2. **Verify 5V voltage** under load (should be >4.8V)
3. **Don't use GPIO 14/15** - Reserved for camera

### WiFi Not Connecting
1. **Verify SSID and password** in code
2. **Check 2.4GHz WiFi** - ESP32 doesn't support 5GHz
3. **Move closer to router** during testing

---

## 📊 I2C Address Reference

| Device | Default Address | Modified Address | How to Change |
|--------|----------------|------------------|---------------|
| MPU6050 | 0x68 | 0x69 | Connect AD0 to 3.3V |
| DS3231 | 0x68 | Cannot change | Fixed at 0x68 |

**⚠️ This is why MPU6050's AD0 MUST be connected to 3.3V!**

---

## 🎯 Final Pin Summary

```
ESP32-CAM GPIO Pins Used:
├─ GPIO 2  → I2C SDA (MPU6050 + RTC)
├─ GPIO 4  → I2C SCL (MPU6050 + RTC)
├─ GPIO 12 → GPS RX (receives from GPS TX)
├─ GPIO 13 → GPS TX (sends to GPS RX)
└─ GPIO 0,1,3,5,14,15,16 → RESERVED FOR CAMERA (do not use!)

Power Distribution:
├─ 5V  → RTC, GPS, ESP32-CAM
├─ 3.3V → MPU6050, Pull-up resistors
└─ GND → Common ground for ALL modules
```

---

## 📝 Notes

- **Camera Quality:** Set `FRAMESIZE_QVGA` for balance of quality/speed
- **GPS Baud Rate:** 9600 is standard (some modules use 115200)
- **RTC Battery:** CR2032 coin cell keeps time when powered off
- **MPU6050 Range:** 4G range is suitable for most applications
- **Debugging:** Serial Monitor at 115200 baud shows all status messages

---

## ✅ Success Indicators

When everything is working, Serial Monitor should show:
```
I2C initialized on GPIO2 (SDA) & GPIO4 (SCL)
Scanning for MPU6050... Found at 0x69 ✓
Initializing RTC DS3231... OK ✓
GPS initialized on GPIO12/13
Initializing camera... OK ✓
WiFi connected ✓
IP Address: 192.168.x.x
HTTP server started successfully
```

---

**Created for ESP32-CAM Pothole Detection System**
*Last Updated: 2026-02-04*