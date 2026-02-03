import cv2
import os
import glob
import csv
import urllib.request
import json
import time
import math
from typing import List, Tuple, Dict, Set

from pothole_detection.detector import PotholeDetector
from pothole_detection.tracker import PotholeTracker

# --- Path Configuration ---
# Get the directory where this script is located (i.e., .../Project/src)
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# Get project root (i.e., .../Project)
ROOT_DIR = os.path.dirname(BASE_DIR)

ASSETS_DIR = os.path.join(ROOT_DIR, 'assets')
OUTPUTS_DIR = os.path.join(ROOT_DIR, 'outputs')

VIDEO_DIR = os.path.join(ASSETS_DIR, 'videos')
MODEL_PATH = os.path.join(ASSETS_DIR, 'models', 'pothole_yolov8.pt')
OUTPUT_VIDEO_PATH = os.path.join(OUTPUTS_DIR, 'videos', 'output_pothole_detection.mp4')
LOG_PATH = os.path.join(OUTPUTS_DIR, 'logs', 'pothole_log.csv')

# Detection / Tracking Parameters
CONF_THRESHOLD = 0.25  # LOCKED by user requirement
TRACKER_MAX_AGE = 30
TRACKER_MIN_HITS = 3
TRACKER_IOU_THRESH = 0.3

# Physics / Sensor Parameters
J_MIN = 0.0
J_MAX = 20.0
SENSOR_URL = "http://192.168.4.1/sensors"  # Placeholder URL for ESP32


# Visualization
REFERENCE_LINE_RATIO = 0.75  # Position of line (0.0 to 1.0)
FONT = cv2.FONT_HERSHEY_SIMPLEX


def get_sensor_burst(url: str, burst_count: int = 5) -> Tuple[float, str, str, float, float]:
    """
    Fetches a burst of sensor data to compute peak jerk and get RTC time.
    Returns: (peak_jerk, date_str, time_str, lat, lon)
    """
    accel_magnitudes = []
    timestamps = []
    
    last_date = "2000-01-01"
    last_time = "00:00:00"
    last_lat = 0.0
    last_lon = 0.0

    # FALLBACK defaults for when hardware is not present
    fallback_date = time.strftime("%Y-%m-%d")
    fallback_time = time.strftime("%H:%M:%S")

    print(f"  > Querying sensors (Burst {burst_count})...")
    
    for _ in range(burst_count):
        try:
            # Mocking the request for the script to run without live hardware
            # In production, uncomment the following lines:
            # with urllib.request.urlopen(url, timeout=0.5) as response:
            #     data = json.loads(response.read().decode())
            #     ax, ay, az = data['ax'], data['ay'], data['az']
            #     accel_magnitudes.append(math.sqrt(ax**2 + ay**2 + az**2))
            #     last_date = data['date']
            #     last_time = data['time']
            #     last_lat = data['lat']
            #     last_lon = data['lon']
            
            # Simulated Data for Logic Verification
            accel_magnitudes.append(9.8 + (0.5 * _)) # Simulate slight movement
            last_date = fallback_date
            last_time = fallback_time
            time.sleep(0.05) # Small delay between samples
            
        except Exception as e:
            # print(f"Sensor read error: {e}")
            pass

    # Compute Jerk
    if len(accel_magnitudes) < 2:
        peak_jerk = 0.0
    else:
        jerks = []
        dt = 0.05 # Assumed fixed dt for burst
        for i in range(1, len(accel_magnitudes)):
            j = abs(accel_magnitudes[i] - accel_magnitudes[i-1]) / dt
            jerks.append(j)
        peak_jerk = max(jerks) if jerks else 0.0

    return peak_jerk, last_date, last_time, last_lat, last_lon


def calculate_severity(confidence: float, jerk: float) -> float:
    """
    Calculates severity based on ML confidence (Primary) and Jerk (Secondary).
    Formula: 0.7 * confidence + 0.3 * (jerk_norm ^ 2)
    """
    # Normalize Jerk
    jerk_norm = (jerk - J_MIN) / (J_MAX - J_MIN)
    jerk_norm = max(0.0, min(1.0, jerk_norm)) # Clamp 0-1
    
    if confidence < CONF_THRESHOLD:
        return 0.0
        
    severity = 0.7 * confidence + 0.3 * (jerk_norm ** 2)
    return round(severity, 2)


def draw_visuals(frame: cv2.VideoWriter, 
                 tracks: List[List[float]], 
                 detections: List[List[float]], 
                 logged_ids: Set[int], 
                 final_severities: Dict[int, float], 
                 ref_y: int, 
                 width: int, 
                 height: int,
                 track_id_colors: Dict[int, Tuple[int, int, int]]) -> None:
    """
    Draws bounding boxes, labels, and reference line on the frame.
    """
    # Draw Reference Line
    cv2.line(frame, (0, ref_y), (width, ref_y), (0, 255, 0), 2)

    for track in tracks:
        x1, y1, x2, y2, track_id_float = track
        x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
        track_id = int(track_id_float)

        # Determine color for this ID
        if track_id not in track_id_colors:
             track_id_colors[track_id] = ((track_id * 123) % 255, (track_id * 53) % 255, (track_id * 211) % 255)
        color = track_id_colors[track_id]

        # Find matching detection confidence
        # (Simplified IoU matching just for display purposes)
        best_conf = 0.0
        max_iou = 0
        for det in detections:
            dx1, dy1, dx2, dy2, dconf = det
            xx1 = max(x1, dx1)
            yy1 = max(y1, dy1)
            xx2 = min(x2, dx2)
            yy2 = min(y2, dy2)
            w = max(0, xx2 - xx1)
            h = max(0, yy2 - yy1)
            inter = w * h
            area_track = (x2 - x1) * (y2 - y1)
            area_det = (dx2 - dx1) * (dy2 - dy1)
            union = area_track + area_det - inter
            if union > 0:
                iou = inter / union
                if iou > max_iou:
                    max_iou = iou
                    best_conf = dconf
        
        conf_str = f"{best_conf:.2f}" if max_iou > 0.5 else "Pred"

        # Severity Display Logic
        severity_display = "N/A"
        if track_id in final_severities:
            severity_display = f"{final_severities[track_id]:.2f}"
        
        # Draw Box and Text
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        label = f"ID:{track_id} Conf:{conf_str} Sev:{severity_display}"
        cv2.putText(frame, label, (x1, y1 - 10), FONT, 0.5, color, 2)


def main():
    # 1. Setup Paths
    print(f"Searching for videos in: {VIDEO_DIR}")
    video_search_pattern = os.path.join(VIDEO_DIR, '*.mp4')
    video_files = glob.glob(video_search_pattern)
    if not video_files:
        print(f"Error: No video file found in '{VIDEO_DIR}' directory.")
        return
    video_path = video_files[0]
    
    print(f"Loading model from: {MODEL_PATH}")

    # 2. Initialize Components
    print("Initializing Detector and Tracker...")
    try:
        detector = PotholeDetector(MODEL_PATH, conf_thres=CONF_THRESHOLD)
        tracker = PotholeTracker(max_age=TRACKER_MAX_AGE, min_hits=TRACKER_MIN_HITS, iou_threshold=TRACKER_IOU_THRESH)
    except Exception as e:
        print(f"Failed to initialize components: {e}")
        return

    # 3. Video Capture Setup
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print(f"Error: Could not open video {video_path}")
        return

    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)

    # 4. Output Setup
    # Ensure output directories exist
    os.makedirs(os.path.dirname(OUTPUT_VIDEO_PATH), exist_ok=True)
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(OUTPUT_VIDEO_PATH, fourcc, fps, (width, height))
    
    csv_file = open(LOG_PATH, 'w', newline='')
    csv_writer = csv.writer(csv_file)
    # UPDATED CSV HEADER
    csv_writer.writerow(['date', 'time', 'frame_id', 'pothole_id', 'confidence', 
                         'bounding_box_area', 'aspect_ratio', 'peak_jerk', 'severity', 
                         'latitude', 'longitude'])

    # State variables
    logged_ids = set()
    track_history = {} # For temporal persistence check (Dumper rejection)
    final_severities = {}
    track_id_colors = {}
    ref_y = int(REFERENCE_LINE_RATIO * height)
    
    print(f"Processing started: {video_path}")
    print(f"Output video: {OUTPUT_VIDEO_PATH}")
    print(f"Output log: {LOG_PATH}")
    print("Press 'q' to stop processing early.")

    frame_idx = 0
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        frame_idx += 1
        
        # --- Process Frame ---
        
        # 1. Detect
        detections = detector.detect(frame)
        
        # 2. Track
        tracks = tracker.update(detections)
        
        # 3. Logic & Logging
        for track in tracks:
            x1, y1, x2, y2, track_id_float = track
            track_id = int(track_id_float)
            
            # --- DUMPER / SPEED BREAKER REJECTION LOGIC ---
            
            # 1. Update Track History (Persistence)
            if track_id not in track_history:
                track_history[track_id] = 0
            track_history[track_id] += 1
            
            # 2. Geometry Checks
            bbox_w = x2 - x1
            bbox_h = y2 - y1
            frame_area = width * height
            bbox_area = bbox_w * bbox_h
            
            area_ratio = bbox_area / frame_area
            aspect_ratio = bbox_w / bbox_h if bbox_h > 0 else 0
            
            # Check 1: Area too large? (Dumper)
            if area_ratio > 0.25:
                continue # Reject as dumper
                
            # Check 2: Aspect ratio too wide? (Speed breaker/Dumper)
            if aspect_ratio > 3.0:
                continue # Reject
                
            # Check 3: Persistence too long? (Static object/Dumper)
            if track_history[track_id] > 10:
                continue # Reject
            
            # ---------------------------------------------

            # Check for crossing / severity calculation
            cy = (y1 + y2) / 2
            
            # Only proceed if passing reference line and VALID pothole (not dumper)
            if cy >= ref_y and track_id not in logged_ids:
                
                # Retrieve Best Confidence for this ID
                best_conf_log = 0.0
                max_iou_log = 0
                for det in detections:
                    dx1, dy1, dx2, dy2, dconf = det
                    xx1 = max(x1, dx1); yy1 = max(y1, dy1); xx2 = min(x2, dx2); yy2 = min(y2, dy2)
                    w_i, h_i = max(0, xx2 - xx1), max(0, yy2 - yy1)
                    inter = w_i * h_i
                    union = ((x2-x1)*(y2-y1)) + ((dx2-dx1)*(dy2-dy1)) - inter
                    if union > 0 and (inter/union) > max_iou_log:
                        max_iou_log = inter/union
                        best_conf_log = dconf
                
                # CONFIDENCE GATE
                if best_conf_log < CONF_THRESHOLD:
                    continue # Ignore completely
                
                # --- SENSOR QUERY (BURST) ---
                # Only query sensors now that we have a confirmed valid pothole
                peak_jerk, rtc_date, rtc_time, lat, lon = get_sensor_burst(SENSOR_URL)
                
                # --- SEVERITY CALCULATION ---
                severity_val = calculate_severity(best_conf_log, peak_jerk)
                
                # --- CSV LOGGING ---
                csv_writer.writerow([
                    rtc_date, rtc_time, frame_idx, track_id, f"{best_conf_log:.2f}",
                    bbox_area, f"{aspect_ratio:.2f}", f"{peak_jerk:.2f}", f"{severity_val:.2f}",
                    lat, lon
                ])
                csv_file.flush() # Ensure data is written immediately
                
                logged_ids.add(track_id)
                final_severities[track_id] = severity_val

        # 4. Visualize
        draw_visuals(frame, tracks, detections, logged_ids, final_severities, ref_y, width, height, track_id_colors)
        
        # Draw Total Count
        total_potholes = tracker.get_total_count()
        cv2.putText(frame, f"Total Unique Potholes: {total_potholes}", (20, 40), FONT, 1, (0, 0, 255), 2)

        out.write(frame)
        # cv2.imshow('Pothole Detection', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
            
    # Cleanup
    cap.release()
    out.release()
    csv_file.close()
    cv2.destroyAllWindows()
    print("Processing complete.")

if __name__ == "__main__":
    main()
