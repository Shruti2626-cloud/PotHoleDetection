# Intelligent Pothole Detection System (IPDS)

An advanced IoT and AI-powered solution for real-time road hazard assessment using a dual ESP32 architecture and YOLOv8 object detection.

---

## Problem Statement
Poorly maintained roads and unexpected potholes cause severe vehicle damage and contribute to thousands of traffic accidents annually. Current road inspection methods rely heavily on slow, manual reporting by citizens or municipal workers, leaving prolonged periods where hazardous road conditions go unaddressed.

## Solution Overview
The IPDS project solves this by automating road quality assessment. By mounting a camera and accelerometer setup on a vehicle, the system continuously scans the road ahead. When the computer vision model detects a pothole, it triangulates that visual data with physical vibration data (jerk) captured as the vehicle drives over it. This creates a highly accurate, geo-tagged "Severity Score" for every pothole, ready to be mapped and prioritized by road maintenance authorities.

## Key Features
- **Real-time Pothole Detection:** Powered by a custom-trained YOLOv8 model.
- **Multi-Object Tracking:** Uses the SORT algorithm to prevent duplicate counting of the same pothole across frames.
- **Vehicle Vibration Sensing:** MPU6050 accelerometer accurately measures the physical depth/impact of the pothole.
- **GPS Location Tagging:** Automatically records the exact latitude and longitude of hazards.
- **Sensor Fusion Severity Scoring:** Combines AI visual confidence with physical impact data to rank pothole severity.
- **Smart Data Logging:** Outputs clean, Firebase-ready CSV logs.

---

## System Overview (High Level)
The system operates on an **Event-Driven Dual ESP32 Architecture**. 
1. The **ESP32-CAM** continuously streams video to a central Python processing hub.
2. The Python hub runs **YOLOv8** to visually detect potholes.
3. Upon confirming a valid pothole, Python sends a query to the **ESP32 Sensor Node**.
4. The Sensor Node responds with physical accelerometer, GPS, and RTC data.
5. The system calculates a combined severity score and logs the data.

## Hardware Used
- **Vision Node:** ESP32-CAM + OV2640 Module
- **Sensor Node:** ESP32 Dev Board
- **Accelerometer:** MPU6050 (6-axis motion tracking)
- **GPS Module:** NEO-6M
- **Real-Time Clock:** DS3231 RTC
- **Processing Hub:** Laptop or Edge Computer (e.g., Jetson Nano)

## Software Stack
- **Python 3.10+** (Core processing logic)
- **OpenCV** (Image processing and video stream handling)
- **Ultralytics YOLOv8** (Computer Vision / Object Detection)
- **FilterPy / SciPy** (SORT tracking algorithm)
- **C++ / Arduino Framework** (ESP32 Firmware)

---

## Project Structure
```text
Pot Hole Detection/
├── .env                             # Centralized WiFi credentials
├── README.md                        # This file
├── update_wifi.py                   # Script to inject WiFi credentials to C++
├── requirements.txt                 # Python dependencies
├── python/                          
│   ├── main.py                      # Main AI & Processing entry point
│   └── pothole_detection/           # SORT and YOLO wrapper modules
├── ESP_32_Code/                        
│   ├── esp_32_cam_final/            # ESP32-CAM firmware (Vision Node)
│   └── esp_32_final/                # ESP32 Sensor firmware (Sensor Node)
├── assets/
│   └── models/                      # Trained YOLOv8 model (.pt)
├── outputs/                         # Generated CSV logs and annotated videos
└── docs/                            # Deep technical documentation
```

---

## Installation & Setup Instructions

### 1. Hardware Configuration
1. Open the `.env` file in the root directory and update it with your WiFi Hotspot's SSID and Password. All devices *must* share the same network.
2. Run `python update_wifi.py` to sync these credentials to the ESP32 code.
3. Flash `ESP_32_Code/esp_32_cam_final/esp_32_cam_final.ino` to your ESP32-CAM. Monitor the Serial output to get its IP address.
4. Flash `ESP_32_Code/esp_32_final/esp_32_final.ino` to your ESP32 Sensor Node. Monitor the Serial output to get its IP address.

### 2. Software Configuration
1. Install dependencies: `pip install -r requirements.txt`
2. Open `python/main.py`.
3. Locate the `ESP32_CAM_IP` and `ESP32_SENSOR_IP` variables at the top of the file. Update them with the IPs you retrieved from the hardware step.

## Usage
To start the live tracking and detection system, simply run:
```bash
python python/main.py
```
A video window will open showing the live feed with bounding boxes and tracking IDs. Press `q` to safely terminate the program. All reports will be saved in the `outputs/logs/` directory.

---

## Future Scope
- **Dashcam Integration:** Utilize ubiquitous vehicle dashcams and smartphone apps to passively crowdsource road condition data without custom hardware.
- **System Scalability:** Migrate processing to distributed cloud infrastructure (AWS/GCP) using microservices and Kubernetes to handle millions of edge devices.
- **Government Portals:** Provide secure web dashboards for municipal bodies featuring real-time heatmaps and predictive analytics for budget allocation.
- **Navigational Alerts:** Integrate with OpenStreetMap (OSM) to issue proactive audio/visual warnings to drivers about upcoming road hazards.
- **Spatial Database:** Store detections in a highly optimized database (e.g., PostgreSQL with PostGIS) and expose REST APIs for researchers and self-driving models.
- **Advanced Sensor Integration:** Incorporate LiDAR or Thermal/Infrared cameras for 24/7 visibility regardless of weather, glare, or darkness.
- **Predictive Maintenance:** Analyze micro-cracks and freeze-thaw weather patterns to predict and map impending road failures before physical potholes form.
- **V2X Communication:** Broadcast real-time hazard coordinates to trailing autonomous vehicles so they can automatically steer around or brace for impact.
- **Automated Work-Orders:** Integrate strictly with City ERP software to automatically dispatch repair crews and calculate necessary asphalt volumes for severe hazards.
- **True Edge Acceleration:** Utilize dedicated AI ASICs (like Google Coral TPU or Hailo-8) to run the entire YOLO pipeline locally without needing a laptop.
- **Crowdsourcing Incentives:** Implement gamified "micro-bounties" (e.g., toll credits or tax rebates) to incentivize citizens to run the detection app while commuting.

## Contributors
- **Sanskar Tiwari**
- **Shruti Shinde**
- **Swarali Bansod**
- **Eshwari Kognole** 
