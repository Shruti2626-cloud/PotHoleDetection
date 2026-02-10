# Dual ESP32 Wiring Guide
## ESP32-CAM Vision Node + ESP32 Sensor Node

**Architecture:** Dual ESP32 (v2.0)  
**Last Updated:** 2026-02-10

---

## ⚠️ CRITICAL NOTES

1. **Dual ESP32 Architecture:**
   - **ESP32-CAM Vision Node:** Camera streaming only (no sensors)
   - **ESP32 Sensor Node:** MPU6050 + GPS + RTC (no camera)

2. **I2C Address Conflict (Sensor Node Only):**
   - MPU6050 and DS3231 both use address `0x68` by default
   - **SOLUTION:** Connect MPU6050's AD0 pin to 3.3V → Changes address to `0x69`
   - **CRITICAL:** This is mandatory for both sensors to work!

3. **Pull-Up Resistors Required:** I2C bus needs 4.7kΩ pull-ups on SDA and SCL

4. **Power Requirements:**
   - ESP32-CAM: **5V 2A** (camera needs high current)
   - ESP32 Sensor Node: **5V 1A** (sensors use less power)

5. **Common Ground:** All components MUST share common ground

6. **WiFi Network:** Both ESP32 devices must connect to the same WiFi network

---

## 📋 Components List

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32-CAM (AI Thinker) | 1 | Vision Node - camera streaming |
| ESP32 DevKit | 1 | Sensor Node - MPU6050 + GPS + RTC |
| MPU6050 | 1 | 6-axis accelerometer/gyroscope |
| DS3231 RTC | 1 | Real-time clock module |
| GPS Module (NEO-6M) | 1 | UART GPS with TX/RX |
| 4.7kΩ Resistors | 2 | I2C pull-ups (can use 2.2kΩ - 10kΩ) |
| Jumper Wires | Multiple | For connections |
| 5V Power Supply | 2 | 2A for ESP32-CAM, 1A for Sensor Node |
| Breadboard | 1-2 | Optional, for prototyping |

---

## 🔌 Pin Assignments

### ESP32-CAM Vision Node (Camera Only)

| Function | ESP32-CAM GPIO | Connected To |
|----------|----------------|--------------|
| Camera | GPIO 0,1,3,5,14,15,16 + others | OV2640 Camera (built-in) |
| 5V | 5V pin | Power supply positive |
| GND | GND | Power supply negative |
| **No sensors connected** | - | All sensors on separate ESP32 |

### ESP32 Sensor Node (Sensors Only)

| Function | ESP32 GPIO | Connected To |
|----------|------------|--------------|
| I2C SDA | GPIO 21 | MPU6050 SDA + RTC SDA + Pull-up |
| I2C SCL | GPIO 22 | MPU6050 SCL + RTC SCL + Pull-up |
| GPS RX | GPIO 16 (RX2) | GPS TX |
| GPS TX | GPIO 17 (TX2) | GPS RX |
| 5V | 5V pin | Power supply positive |
| 3.3V | 3.3V pin | MPU6050 VCC + Pull-up resistors |
| GND | GND | Common ground for all modules |

---

## 🔧 Component Wiring

### Node 1: ESP32-CAM Vision Node

#### Power
```
Power Supply (5V, 2A)
├─ Positive (+) ──────→ ESP32-CAM 5V pin
└─ Negative (-) ──────→ ESP32-CAM GND pin
```

#### Camera
```
OV2640 Camera: Built-in to ESP32-CAM module
No external wiring needed
```

**That's it for Vision Node!** No sensors connected.

---

### Node 2: ESP32 Sensor Node

#### Power
```
Power Supply (5V, 1A)
├─ Positive (+) ──────→ ESP32 5V pin (or VIN)
└─ Negative (-) ──────→ ESP32 GND pin
```

#### 1. MPU6050 (Accelerometer/Gyroscope)
```
MPU6050          ESP32 DevKit
─────────────────────────────
VCC    ──────→   3.3V
GND    ──────→   GND
SCL    ──────→   GPIO 22 (I2C SCL)
SDA    ──────→   GPIO 21 (I2C SDA)
AD0    ──────→   3.3V  ⚠️ CRITICAL: Sets address to 0x69
INT    ──────→   (Not connected)
```

**Notes:**
- MPU6050 operates at **3.3V**
- **AD0 to 3.3V is MANDATORY** to avoid address conflict with RTC
- If AD0 is left floating or connected to GND, address will be 0x68 (conflict!)

#### 2. DS3231 RTC (Real-Time Clock)
```
DS3231           ESP32 DevKit
─────────────────────────────
VCC    ──────→   3.3V (or 5V, module accepts both)
GND    ──────→   GND
SCL    ──────→   GPIO 22 (I2C SCL) - shared with MPU6050
SDA    ──────→   GPIO 21 (I2C SDA) - shared with MPU6050
32K    ──────→   (Not connected)
SQW    ──────→   (Not connected)
```

**Notes:**
- DS3231 has I2C address **0x68** (fixed, cannot change)
- Most DS3231 modules have **built-in pull-up resistors**
- Battery backup keeps time when power is off

#### 3. GPS Module (NEO-6M)
```
GPS Module       ESP32 DevKit
─────────────────────────────
VCC    ──────→   3.3V (or 5V, check your module)
GND    ──────→   GND
TX     ──────→   GPIO 16 (ESP32 RX2)
RX     ──────→   GPIO 17 (ESP32 TX2)
PPS    ──────→   (Not connected)
```

**Notes:**
- GPS TX connects to ESP32 RX (data flows GPS → ESP32)
- GPS RX connects to ESP32 TX (data flows ESP32 → GPS)
- Some GPS modules need 5V, others work with 3.3V - check your module!
- GPS needs **clear view of sky** for satellite lock

#### 4. I2C Pull-Up Resistors ⚠️ REQUIRED
```
              3.3V
                │
                ├──[ 4.7kΩ ]──→ GPIO 21 (SDA)
                │
                └──[ 4.7kΩ ]──→ GPIO 22 (SCL)
```

**Notes:**
- I2C **requires** pull-up resistors
- Use 4.7kΩ (or anywhere from 2.2kΩ to 10kΩ)
- Connect to **3.3V**, not 5V
- Some modules have built-in pull-ups, but adding external ones ensures reliable operation

---

## 📐 Complete Wiring Diagram (Dual ESP32 Architecture)

```
┌─────────────────────────────────────────────────────────────┐
│                    VEHICLE DASHBOARD                         │
│                                                              │
│  ┌──────────────────┐              ┌──────────────────┐     │
│  │ ESP32-CAM        │              │ ESP32 DevKit     │     │
│  │ Vision Node      │              │ Sensor Node      │     │
│  │                  │              │                  │     │
│  │ • OV2640 Camera  │              │ • MPU6050 (0x69) │     │
│  │ • MJPEG Stream   │              │ • GPS NEO-6M     │     │
│  │ • Port 81        │              │ • RTC DS3231     │     │
│  │ • NO SENSORS     │              │ • Port 80        │     │
│  └────────┬─────────┘              └────────┬─────────┘     │
│           │                                 │               │
│           │ WiFi                            │ WiFi          │
│           └────────┬────────────────────────┘               │
│                    │                                        │
│           ┌────────▼────────┐                               │
│           │  WiFi Router    │                               │
│           │  (Hotspot)      │                               │
│           └────────┬────────┘                               │
│                    │                                        │
│           ┌────────▼────────┐                               │
│           │ Laptop/PC       │                               │
│           │ Python Hub      │                               │
│           │                 │                               │
│           │ • YOLOv8        │                               │
│           │ • SORT          │                               │
│           │ • CSV Logger    │                               │
│           └─────────────────┘                               │
└─────────────────────────────────────────────────────────────┘

ESP32-CAM Vision Node:
┌─────────────────────┐
│  5V Power (2A)      │
└──────┬──────────────┘
       │
┌──────▼──────────────┐
│  ESP32-CAM          │
│  ┌──────────────┐   │
│  │  OV2640      │   │
│  │  Camera      │   │
│  └──────────────┘   │
│                     │
│  WiFi: 192.168.x.x  │
│  Port: 81           │
└─────────────────────┘

ESP32 Sensor Node:
┌─────────────────────┐
│  5V Power (1A)      │
└──────┬──────────────┘
       │
┌──────▼──────────────────────────────┐
│  ESP32 DevKit                       │
│                                     │
│  GPIO 21 (SDA) ●────┬───────┬──────┤
│  GPIO 22 (SCL) ●────┼───┬───┼──────┤
│  GPIO 16 (RX2) ●────┼───┼───┼──────┼──→ GPS TX
│  GPIO 17 (TX2) ●────┼───┼───┼──────┼──→ GPS RX
│  3.3V ●─────────────┼───┼───┼──────┼──┐
│  GND ●──────────────┼───┼───┼──────┼──┼──→ Common GND
└─────────────────────┼───┼───┼──────┼──┘
                      │   │   │      │
         ┌────────────┘   │   │      │
         │                │   │      │
    ┌────▼─────┐     ┌────▼───▼──┐  │
    │ MPU6050  │     │ DS3231    │  │
    │ (0x69)   │     │ RTC       │  │
    │          │     │ (0x68)    │  │
    │ AD0→3.3V │     │           │  │
    └──────────┘     └───────────┘  │
                                    │
                          ┌─────────▼──┐
                          │ GPS NEO-6M │
                          │            │
                          └────────────┘
```

---

## 🧪 Testing Checklist

### Before Powering On:

**Vision Node (ESP32-CAM):**
- [ ] Camera ribbon cable properly seated
- [ ] 5V 2A power supply connected
- [ ] No short circuits
- [ ] WiFi credentials configured in firmware

**Sensor Node (ESP32):**
- [ ] All GND connections are common
- [ ] Pull-up resistors installed (4.7kΩ on SDA and SCL to 3.3V)
- [ ] **MPU6050 AD0 pin connected to 3.3V** (CRITICAL!)
- [ ] GPS antenna has clear view of sky
- [ ] RTC has CR2032 battery installed
- [ ] 5V 1A power supply connected
- [ ] WiFi credentials configured in firmware

### After Powering On:

**Vision Node:**
- [ ] Serial Monitor shows "Vision Node Ready" (115200 baud)
- [ ] WiFi connects and shows IP address
- [ ] Camera initializes successfully
- [ ] Watchdog timer initialized
- [ ] Can access `http://[IP]:81/stream` in browser

**Sensor Node:**
- [ ] Serial Monitor shows "Sensor Node Ready" (115200 baud)
- [ ] WiFi connects and shows IP address
- [ ] MPU6050 detected at **0x69** (not 0x68!)
- [ ] RTC detected at 0x68
- [ ] GPS starts receiving data (may take 30-60 seconds)
- [ ] Can access `http://[IP]/health` endpoint

### Integration Test:
- [ ] Both ESP32 devices on same WiFi network
- [ ] Python script can connect to both endpoints
- [ ] Video stream displays correctly
- [ ] Sensor queries return valid JSON data

---

## 🔍 Troubleshooting

### Vision Node Issues

**Camera Not Working:**
1. Check 5V power supply (needs 2A minimum)
2. Verify camera ribbon cable is properly seated
3. Check Serial Monitor for initialization errors
4. Try different JPEG quality settings

**Stream Not Accessible:**
1. Verify WiFi connection (check Serial Monitor)
2. Test URL in browser: `http://[IP]:81/stream`
3. Check firewall settings
4. Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)

### Sensor Node Issues

**I2C Devices Not Found:**
1. **Check pull-up resistors** - Most common issue!
2. **Verify MPU6050 AD0 pin** - Must be connected to 3.3V
3. Verify wiring - Especially SDA/SCL not swapped
4. Test with I2C scanner code
5. Check power - Ensure 3.3V is stable

**MPU6050 Found at 0x68 (Wrong Address):**
- **PROBLEM:** AD0 pin not connected to 3.3V
- **SOLUTION:** Connect AD0 to 3.3V to change address to 0x69
- **WARNING:** Will conflict with RTC if left at 0x68!

**GPS Not Getting Fix:**
1. Position GPS antenna near window or outdoors
2. Wait 30-60 seconds for satellite lock (cold start)
3. Check TX/RX not swapped - GPS TX → ESP32 GPIO 16
4. Verify GPS module power (some need 5V, others 3.3V)

**RTC Not Found:**
1. Check I2C wiring (shared with MPU6050)
2. Verify I2C address (0x68)
3. Check CR2032 battery is installed
4. Ensure pull-up resistors are present

### WiFi Issues (Both Nodes)

1. Verify SSID and password in code
2. Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
3. Move closer to router during testing
4. Check Serial Monitor for connection status

---

## 📊 I2C Address Reference

| Device | Default Address | Modified Address | How to Change |
|--------|----------------|------------------|---------------|
| MPU6050 | 0x68 | **0x69** ✓ | Connect AD0 to 3.3V |
| DS3231 | 0x68 | Cannot change | Fixed at 0x68 |

**⚠️ This is why MPU6050's AD0 MUST be connected to 3.3V!**

---

## 🎯 Final Pin Summary

### ESP32-CAM Vision Node
```
GPIO Pins Used:
├─ GPIO 0,1,3,5,14,15,16 → RESERVED FOR CAMERA (do not use!)
└─ All other pins → Available but unused

Power:
├─ 5V  → ESP32-CAM + Camera
└─ GND → Common ground

Network:
└─ WiFi → Port 81 (MJPEG stream)
```

### ESP32 Sensor Node
```
GPIO Pins Used:
├─ GPIO 21 → I2C SDA (MPU6050 + RTC)
├─ GPIO 22 → I2C SCL (MPU6050 + RTC)
├─ GPIO 16 → GPS RX (receives from GPS TX)
└─ GPIO 17 → GPS TX (sends to GPS RX)

Power Distribution:
├─ 5V    → ESP32, RTC, GPS
├─ 3.3V  → MPU6050, Pull-up resistors
└─ GND   → Common ground for ALL modules

I2C Addresses:
├─ MPU6050 → 0x69 (AD0 = HIGH)
└─ DS3231  → 0x68 (fixed)

Network:
└─ WiFi → Port 80 (sensor queries)
```

---

## 📝 Important Notes

### Dual ESP32 Benefits:
- **Separation of Concerns:** Camera and sensors on different devices
- **No GPIO Conflicts:** ESP32-CAM's limited pins only used for camera
- **Event-Driven Sensors:** 95% power savings vs continuous streaming
- **Independent Failure:** If one node fails, other continues working

### Power Recommendations:
- **Vision Node:** Use dedicated 5V 2A power supply (camera draws significant current)
- **Sensor Node:** 5V 1A sufficient (sensors use minimal power)
- **Avoid USB Power:** May not provide stable current for camera

### WiFi Configuration:
- Both nodes must connect to **same WiFi network**
- Python hub must be on **same network**
- Use **2.4GHz WiFi** (ESP32 doesn't support 5GHz)

---

## ✅ Success Indicators

### Vision Node Serial Output:
```
========================================
ESP32-CAM VISION NODE
Dual ESP32 Architecture v2.0
========================================

Initializing watchdog timer (30s)... OK ✓
Initializing OV2640 camera... SUCCESS ✓
Connecting to WiFi... CONNECTED ✓
IP Address: 192.168.x.x
Starting stream server on port 81... SUCCESS ✓

========================================
VISION NODE READY
========================================
Endpoints:
  MJPEG Stream: http://192.168.x.x:81/stream
  Health Check: http://192.168.x.x:81/health
========================================
```

### Sensor Node Serial Output:
```
========================================
ESP32 SENSOR NODE
Dual ESP32 Architecture v2.0
========================================

I2C initialized on GPIO21 (SDA) & GPIO22 (SCL)
Initializing MPU6050... FOUND at 0x69 ✓ (AD0=HIGH, no conflict with RTC)
  Range: ±8g
  Filter: 21 Hz
Initializing NEO-6M GPS... OK (GPIO16/GPIO17)
Initializing DS3231 RTC... FOUND ✓
Connecting to WiFi... CONNECTED ✓
IP Address: 192.168.x.x

========================================
SENSOR NODE READY
========================================
Endpoints:
  Sensor Query: http://192.168.x.x/query?pothole_id=<ID>
  Health Check: http://192.168.x.x/health
========================================
Sensors: MPU=OK GPS=OK RTC=OK
========================================
```

---

**Created for Dual ESP32 Pothole Detection System**  
*Last Updated: 2026-02-10*  
*Architecture: v2.0 (Dual ESP32)*