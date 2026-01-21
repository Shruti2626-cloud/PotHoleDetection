# Pothole Detection System (YOLOv8 + SORT)

This project detects potholes in road videos using YOLOv8 and tracks them using the SORT algorithm to ensure unique counting.

## Features
- **Robust Detection**: Uses YOLOv8 for accurate pothole recognition.
- **Stable Tracking**: Uses SORT (Simple Online and Realtime Tracking) with Kalman Filters to track potholes across frames, making it invariant to minor camera motions and vibrations.
- **Unique Counting**: Counts each pothole exactly once, even if detection flickers.


## Project Structure
- `main.py`: Main video processing loop.
- `detector.py`: YOLOv8 detection logic.
- `tracker.py`: Wrapper for SORT tracking and unique counting.
- `sort.py`: The SORT tracking algorithm.
- `requirements.txt`: List of dependencies.

## How It Works
- **Detection**: We detect potholes in every frame.
- **Predictive Tracking (SORT)**: We don't just match boxes; we predict where the pothole *should* be in the next frame using velocity (Kalman Filter). This means if the camera shakes or the pothole is momentarily missed by YOLO, the tracker "remembers" it.
- **Unique IDs**: We assign a permanent ID to each tracked pothole. We only increment the "Total" counter when a new ID is confirmed.
