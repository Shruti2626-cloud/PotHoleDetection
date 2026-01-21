import cv2
from ultralytics import YOLO
import numpy as np

class PotholeDetector:
    def __init__(self, model_path, conf_thres=0.3):
        """
        Initializes the YOLOv8 detector.
        
        Args:
            model_path (str): Path to the .pt model file.
            conf_thres (float): Confidence threshold for detections.
        """
        self.model = YOLO(model_path)
        self.conf_thres = conf_thres
        # Ensure model runs on GPU if available, else CPU (handled by ultralytics automatically usually, but good to know)
        
    def detect(self, frame):
        """
        Detects potholes in a single frame.
        
        Args:
            frame (np.ndarray): Input image/frame.
            
        Returns:
            np.ndarray: Detections in format [[x1, y1, x2, y2, score], ...]
        """
        results = self.model(frame, verbose=False, conf=self.conf_thres)
        
        detections = []
        
        # YOLOv8 returns a list of Results objects
        for result in results:
            boxes = result.boxes
            for box in boxes:
                # Bounding box coordinates
                x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
                
                # Confidence score
                conf = box.conf[0].cpu().numpy()
                
                # Class ID (optional check, assuming everything detected is a pothole)
                # cls = box.cls[0].cpu().numpy()
                
                detections.append([x1, y1, x2, y2, conf])
                
        return np.array(detections) if detections else np.empty((0, 5))
