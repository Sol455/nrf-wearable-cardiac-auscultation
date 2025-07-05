import matplotlib.pyplot as plt
import numpy as np
    
class PeakDetectorNPoint:
    def __init__(self, block_size, num_blocks, alpha=0.01, threshold_scale=1.5, min_distance=100):
        self.block_size = block_size
        self.num_blocks = num_blocks
        self.samples = [0.0, 0.0, 0.0] 
        self.index = 0                
        self.running_mean = 0.0
        self.alpha = alpha
        self.threshold_scale = threshold_scale
        self.min_distance = min_distance
        self.samples_since_peak = float('inf')  # so it can trigger immediately

    def update(self, x, sample_index_in_block, dbg_global_index, use_external_mean=False, external_mean=0):

        if use_external_mean:
            self.running_mean = external_mean
        else:
            self.running_mean += self.alpha * (x - self.running_mean)

        threshold = self.threshold_scale * self.running_mean
        self.samples[self.index] = x  # Store current sample x[n]

        #Get the 3 most recent samples in correct temporal order:
        i_prev = (self.index - 2) % 3  # x[n-2]
        i_mid  = (self.index - 1) % 3  # x[n-1] peak candidate
        i_next = self.index            # x[n]

        prev = self.samples[i_prev]
        mid  = self.samples[i_mid]
        next = self.samples[i_next]

        is_peak = False
        peak_value = None
        sample_index = None

        if (mid > threshold and mid > prev and mid > next and
                self.samples_since_peak >= self.min_distance):
            is_peak = True
            peak_value = mid
            self.samples_since_peak = 0
        else:
            self.samples_since_peak += 1

        self.index = (self.index + 1) % 3  # Rotate circular buffer

        if is_peak:
            sample_index = sample_index_in_block - 1  # as mid is x[n-1]

            message = {
                "sample_index": sample_index,
                "value": peak_value,
                "global_index": dbg_global_index,
                "type": "unval"
            }
            return message

        return None
    
