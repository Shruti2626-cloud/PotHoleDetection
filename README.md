# Pothole Detection System (YOLOv8 + SORT)

This project detects potholes in road videos using YOLOv8 and tracks them using the SORT algorithm to ensure unique counting.

## Features
- **Robust Detection**: Uses YOLOv8 for accurate pothole recognition.
- **Stable Tracking**: Uses SORT (Simple Online and Realtime Tracking) with Kalman Filters to track potholes across frames, making it invariant to minor camera motions and vibrations.
- **Unique Counting**: Counts each pothole exactly once, even if detection flickers.

## Requirements
- Python 3.10 (Strictly required)
- Windows (Visual Studio Code recommended)

## Setup Instructions

### 1. Create a Virtual Environment
Open your terminal (VS Code Terminal) in this directory (`d:\Projects\Pot Hole Detection`) and run:

```powershell
# Ensure you use Python 3.10. If 'python' is not 3.10, use the full path to python.exe or 'py -3.10'
py -3.10 -m venv venv
```

### 2. Activate the Environment
```powershell
.\venv\Scripts\activate
```

### 3. Install Dependencies
```powershell
pip install -r requirements.txt
```

*Note: This will install `ultralytics`, `opencv-python`, `filterpy`, `scipy`, etc.*

## Usage

1. Ensure your video file is in the `Video` folder.
2. Ensure `pothole_yolov8.pt` is in the `Model` folder.
3. Run the main script:

```powershell
python main.py
```

4. Press `q` to stop the video processing early.
5. The output video will be saved as `output_pothole_detection.mp4`.

## Project Structure
- `main.py`: Main video processing loop.
- `detector.py`: YOLOv8 detection logic.
- `tracker.py`: Wrapper for SORT tracking and unique counting.
- `sort.py`: The SORT tracking algorithm.
- `requirements.txt`: List of dependencies.

## How It Works (Antigravity Logic)
- **Detection**: We detect potholes in every frame.
- **Predictive Tracking (SORT)**: We don't just match boxes; we predict where the pothole *should* be in the next frame using velocity (Kalman Filter). This means if the camera shakes or the pothole is momentarily missed by YOLO, the tracker "remembers" it.
- **Unique IDs**: We assign a permanent ID to each tracked pothole. We only increment the "Total" counter when a new ID is confirmed.
