from collections import deque
import numbers

class TrendAnalyser:
    def __init__(self, buffer_size, slope_thresh, feature_key, timestamp_key, min_windows):
        self.timestamp_key = timestamp_key
        self.buffer_size = buffer_size
        self.slope_thresh = slope_thresh  
        self.feature_key = feature_key    
        self.buffer = deque(maxlen=buffer_size)
        self.event_counter = 0
        self.min_windows = min_windows

    def update(self, event):
        val = event[self.feature_key]
        if not isinstance(val, numbers.Number):
            print(f"Skipping event for {self.feature_key}: {val}")
            return False, 0.0 

        # Use the *real* timestamp
        x = event[self.timestamp_key]
        if not isinstance(x, numbers.Number):
            print(f"Skipping event for {self.timestamp_key}: {x}")
            return False, 0.0
        event['timestamp'] = x  # Now stores real timestamp in ms

        #print(f"BUFFER X: {[e['timestamp'] for e in self.buffer]}")
        self.buffer.append(event)
        n = len(self.buffer)
        if n < self.min_windows:
            return False, 0.0 

        sum_x = sum_y = sum_xy = sum_x2 = 0.0
        for e in self.buffer:
            x = e['timestamp']
            y = e[self.feature_key]
            sum_x += x
            sum_y += y
            sum_xy += x * y
            sum_x2 += x * x

        numerator = n * sum_xy - sum_x * sum_y
        denominator = n * sum_x2 - sum_x * sum_x
        if denominator == 0:
            slope = 0.0
        else:
            slope = numerator / denominator

        alert = slope < self.slope_thresh
        #print(f"RMS BUFFER: {[e[self.feature_key] for e in self.buffer]}")
        return alert, slope 
