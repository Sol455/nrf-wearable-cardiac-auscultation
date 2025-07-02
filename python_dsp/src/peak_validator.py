import numpy as np
from collections import deque

class PeakValidator:
    def __init__(self, send_fn, margin = 0.15, close_r = 0.4, far_r = 0.6):
        self.buffer = deque(maxlen=3)
        self.counter_since_last_peak = 0
        self.send_fn = send_fn
        self.close_r = close_r
        self.far_r= far_r
        self.margin = margin

    def update(self):
        self.counter_since_last_peak += 1

    def notify_peak(self, peak):
        peak["t"] = self.counter_since_last_peak
        self.counter_since_last_peak = 0

        self.buffer.append(peak)
        if len(self.buffer) < 3:
            return  # Not enough peaks yet

        #Oldest to most recent
        p0, p1, p2 = self.buffer

        t1 = p1["t"]
        t2 = p2["t"]
        T = t1 + t2 # Overall est time period

        close_thresh = self.close_r * T
        far_thresh = self.far_r * T

        r1 = t1 / T
        r2 = t2 / T
        lower_bound = 0.5 - self.margin
        upper_bound = 0.5 + self.margin

        #Check for S2
        if t1 < close_thresh and t2 > far_thresh:
            if p0["type"] == "S2":
                p1["type"] = "S1"
                self.send_fn(p1)
                #print("Accept candidate as S1 avoid double S2")   
            else:
                p1["type"] = "S2"
                self.send_fn(p1)
                #print("Rejected candidate as S2")
        #Check for S1 S1 S1
        elif  t1 > close_thresh and t2 < far_thresh:
            #missing S2 therefore S1
            p1["type"] = "S1"
            self.send_fn(p1)
            #print("Accept candidate as S1")
        elif lower_bound < r1 < upper_bound and lower_bound < r2 < upper_bound:
            #missing S2 therefore S1
            p1["type"] = "S1"
            self.send_fn(p1)
            #print("Accept candidate as S1 (S1 S1 S1 case)")
        else:
            p1["type"] = "unval"
            self.send_fn(p1)
            #print("don't accept peak")

