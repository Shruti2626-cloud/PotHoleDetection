import cv2
import os
import glob
import csv
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
CONF_THRESHOLD = 0.3
TRACKER_MAX_AGE = 30
TRACKER_MIN_HITS = 3
TRACKER_IOU_THRESH = 0.3

# Visualization
REFERENCE_LINE_RATIO = 0.75  # Position of line (0.0 to 1.0)
FONT = cv2.FONT_HERSHEY_SIMPLEX


def calculate_severity(w: int, h: int, W: int, H: int) -> float:
    """
    Calculates the severity of a pothole based on its normalized area.
    Formula: min(1.0, (w*h)/(W*H) / 0.08)
    """
    normalized_area = (w * h) / (W * H)
    severity = min(1.0, normalized_area / 0.08)
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
    csv_writer.writerow(['hole_id', 'severity', 'confidence'])

    # State variables
    logged_ids = set()
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
            
            # Check for crossing / severity calculation
            cy = (y1 + y2) / 2
            
            # Calculate and log severity only if passing the reference line and not already logged
            if cy >= ref_y and track_id not in logged_ids:
                bbox_w = x2 - x1
                bbox_h = y2 - y1
                severity_val = calculate_severity(bbox_w, bbox_h, width, height)
                
                # Find best confidence for logging
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
                
                csv_writer.writerow([track_id, severity_val, f"{best_conf_log:.2f}"])
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
