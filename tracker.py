import numpy as np
from sort import Sort

class PotholeTracker:
    def __init__(self, max_age=30, min_hits=3, iou_threshold=0.3):
        """
        Initializes the SORT tracker wrapper.
        
        Args:
            max_age (int): Maximum number of frames to keep a track alive without new detections.
            min_hits (int): Minimum number of hits to start a track (robustness against noise).
            iou_threshold (float): IOU threshold for matching.
        """
        self.tracker = Sort(max_age=max_age, min_hits=min_hits, iou_threshold=iou_threshold)
        self.unique_ids = set()

    def update(self, detections):
        """
        Updates the tracker with new detections.
        
        Args:
            detections (np.ndarray): Detections from detector [[x1, y1, x2, y2, score], ...]
            
        Returns:
            np.ndarray: Tracked objects [[x1, y1, x2, y2, id], ...]
        """
        # SORT expects detections in [x1,y1,x2,y2,score] format
        raw_tracks = self.tracker.update(detections)
        
        tracks = []
        for track in raw_tracks:
            # track is [x1, y1, x2, y2, id]
            # ID is the last element
            pothole_id = int(track[4])
            
            # Robustness: Add to unique count
            if pothole_id not in self.unique_ids:
                self.unique_ids.add(pothole_id)
            
            tracks.append(track)
            
        return np.array(tracks)

    def get_total_count(self):
        """
        Returns the total number of unique potholes detecting so far.
        """
        return len(self.unique_ids)
